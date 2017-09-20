
/*

  KLayout Layout Viewer
  Copyright (C) 2006-2017 Matthias Koefferlein

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "extXORProgress.h"

#include "tlString.h"

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QBitmap>

namespace ext
{

// --------------------------------------------------------------------------------------------------
//  The progress widget class

struct CounterCompare
{
  typedef std::pair<db::LayerProperties, size_t> value_type;
  bool operator() (const value_type &a, const value_type &b) const
  {
    bool a_special = (a.second == missing_in_a || a.second == missing_in_b);
    bool b_special = (b.second == missing_in_a || b.second == missing_in_b);

    if (a_special != b_special) {
      return a_special < b_special;
    }
    if (a.second != b.second) {
      return a.second > b.second;
    }
    return a.first < b.first;
  }
};

class XORProgressWidget
  : public QWidget
{
public:
  XORProgressWidget ()
    : QWidget (0)
  {
    m_pixmap_size = 24;
    m_max_lines = 1;
    m_spacing = 2;

    QFontMetrics fm (font ());
    m_line_height = std::max (fm.height (), m_pixmap_size + 4);
    m_font_height = fm.height () * 3 / 2;
    m_first_column_width = fm.width (QString::fromUtf8 ("LAYERNAME"));
    m_column_width = m_pixmap_size + 4 + m_spacing + fm.width (QString::fromUtf8 ("1.00G "));

    m_ellipsis = false;
  }

  QSize sizeHint () const
  {
    return QSize (int (m_tolerance_labels.size ()) * (m_column_width + m_spacing) + m_first_column_width, (m_line_height + m_spacing) * m_max_lines + m_font_height * 2 + m_spacing);
  }

  void set_results (double dbu, int nx, int ny, const std::map<std::pair<size_t, size_t>, std::map<std::pair<db::LayerProperties, db::Coord>, size_t> > &results, const std::map<db::LayerProperties, size_t> &count_per_layer, const std::vector<db::Coord> &tolerances)
  {
    m_labels.clear ();
    m_layer_labels.clear ();
    m_ellipsis = false;
    m_red_images.clear ();
    m_green_images.clear ();
    m_blue_images.clear ();
    m_yellow_images.clear ();
    m_labels.clear ();

    QSize szh = sizeHint ();

    m_tolerance_labels.clear ();
    for (std::vector<db::Coord>::const_iterator t = tolerances.begin (); t != tolerances.end (); ++t) {
      m_tolerance_labels << tl::to_qstring (tl::sprintf ("%.12g", *t * dbu));
    }

    std::vector<std::pair<db::LayerProperties, size_t> > counters;
    counters.insert (counters.end (), count_per_layer.begin (), count_per_layer.end ());
    std::sort (counters.begin (), counters.end (), CounterCompare ());

    m_max_lines = int (counters.size ());
    m_ellipsis = false;

    int visible_lines = std::max (0, (height () - m_font_height * 2 - m_spacing) / (m_line_height + m_spacing));

    for (std::vector<std::pair<db::LayerProperties, size_t> >::const_iterator c = counters.begin (); c != counters.end (); ++c) {

      if (m_layer_labels.size () == visible_lines) {
        m_ellipsis = true;
        break;
      }

      m_layer_labels << tl::to_qstring (c->first.to_string ());

      m_labels.push_back (QStringList ());
      m_red_images.push_back (std::vector<QImage> ());
      m_green_images.push_back (std::vector<QImage> ());
      m_blue_images.push_back (std::vector<QImage> ());
      m_yellow_images.push_back (std::vector<QImage> ());

      for (std::vector<db::Coord>::const_iterator t = tolerances.begin (); t != tolerances.end (); ++t) {

        m_labels.back ().push_back (QString ());
        m_red_images.back ().push_back (QImage (m_pixmap_size, m_pixmap_size, QImage::Format_MonoLSB));
        m_green_images.back ().push_back (QImage (m_pixmap_size, m_pixmap_size, QImage::Format_MonoLSB));
        m_blue_images.back ().push_back (QImage (m_pixmap_size, m_pixmap_size, QImage::Format_MonoLSB));
        m_yellow_images.back ().push_back (QImage (m_pixmap_size, m_pixmap_size, QImage::Format_MonoLSB));

        m_red_images.back ().back ().fill (Qt::black);
        m_green_images.back ().back ().fill (Qt::black);
        m_blue_images.back ().back ().fill (Qt::black);
        m_yellow_images.back ().back ().fill (Qt::black);

        size_t tot_count = 0;

        for (std::map<std::pair<size_t, size_t>, std::map<std::pair<db::LayerProperties, db::Coord>, size_t> >::const_iterator r = results.begin (); r != results.end (); ++r) {

          const std::map<std::pair<db::LayerProperties, db::Coord>, size_t> &rm = r->second;

          std::map<std::pair<db::LayerProperties, db::Coord>, size_t>::const_iterator rl = rm.find (std::make_pair (c->first, *t));
          if (rl != rm.end ()) {

            tot_count += rl->second;

            QImage *img = 0;
            if (rl->second == 0) {
              img = &m_green_images.back ().back ();
            } else if (rl->second == missing_in_a) {
              m_blue_images.back ().back ().fill (Qt::white);
            } else if (rl->second == missing_in_b) {
              m_yellow_images.back ().back ().fill (Qt::white);
            } else {
              img = &m_red_images.back ().back ();
            }

            if (img) {
              if (nx == 0 || ny == 0) {
                img->fill (Qt::white);
              } else {

                int ix = r->first.first;
                int iy = r->first.second;

                int y2 = m_pixmap_size - 1 - (iy * m_pixmap_size + m_pixmap_size / 2) / ny;
                int y1 = m_pixmap_size - 1 - ((iy + 1) * m_pixmap_size + m_pixmap_size / 2) / ny;
                int x1 = (ix * m_pixmap_size + m_pixmap_size / 2) / nx;
                int x2 = ((ix + 1) * m_pixmap_size + m_pixmap_size / 2) / nx;

                //  "draw" the field
                for (int y = y1; y <= y2 && y >= 0 && y < m_pixmap_size; ++y) {
                  *((uint32_t *) img->scanLine (y)) &= (((1 << x1) - 1) | ~((1 << (x2 + 1)) - 1));
                }

              }
            }

          }

        }

        QString text;
        if (tot_count == missing_in_a) {
          text = QString::fromUtf8 ("B");
        } else if (tot_count == missing_in_b) {
          text = QString::fromUtf8 ("A");
        } else if (tot_count > 1000000000) {
          text = QString::fromUtf8 ("%1G").arg (tot_count * 1e-9, 0, 'f', 2);
        } else if (tot_count > 100000000) {
          text = QString::fromUtf8 ("%1M").arg (tot_count * 1e-6, 0, 'f', 0);
        } else if (tot_count > 10000000) {
          text = QString::fromUtf8 ("%1M").arg (tot_count * 1e-6, 0, 'f', 1);
        } else if (tot_count > 1000000) {
          text = QString::fromUtf8 ("%1M").arg (tot_count * 1e-6, 0, 'f', 2);
        } else if (tot_count > 100000) {
          text = QString::fromUtf8 ("%1k").arg (tot_count * 1e-3, 0, 'f', 0);
        } else if (tot_count > 10000) {
          text = QString::fromUtf8 ("%1k").arg (tot_count * 1e-3, 0, 'f', 1);
        } else if (tot_count > 1000) {
          text = QString::fromUtf8 ("%1k").arg (tot_count * 1e-3, 0, 'f', 2);
        } else {
          text = QString::fromUtf8 ("%1").arg (tot_count);
        }
        m_labels.back ().back () = text;

      }

    }

    if (szh != sizeHint ()) {
      updateGeometry ();
    }

    update ();
  }

  void paintEvent (QPaintEvent * /*ev*/)
  {
    QPainter painter (this);

    int x0 = std::max (0, (width () - sizeHint ().width ()) / 2);
    int visible_columns = std::max (0, (width () - m_first_column_width + 20) / (m_column_width + m_spacing));

    painter.drawText (QRect (QPoint (x0, 0), QSize (m_first_column_width, m_font_height)),
                      tr ("Lay/Tol."),
                      QTextOption (Qt::AlignLeft | Qt::AlignTop));



    for (int t = 0; t < m_tolerance_labels.size () && t < visible_columns; ++t) {
      painter.drawText (QRect (QPoint (x0 + m_first_column_width + m_spacing + t * (m_column_width + m_spacing), 0), QSize (m_column_width, m_font_height)),
                        m_tolerance_labels [t],
                        QTextOption (Qt::AlignLeft | Qt::AlignTop));
    }

    for (int l = 0; l < m_layer_labels.size (); ++l) {

      painter.drawText (QRect (QPoint (x0, m_font_height + m_spacing + l * (m_line_height + m_spacing)), QSize (m_first_column_width, m_line_height)),
                        m_layer_labels [l],
                        QTextOption (Qt::AlignLeft | Qt::AlignVCenter));

      for (int t = 0; t < m_tolerance_labels.size () && t < visible_columns; ++t) {

        int x = x0 + m_first_column_width + m_spacing + t * (m_column_width + m_spacing);
        int y = m_font_height + m_spacing + l * (m_line_height + m_spacing);

        painter.drawText (QRect (QPoint (x + m_pixmap_size + 4 + m_spacing, y), QSize (m_column_width, m_line_height)),
                          m_labels [l][t],
                          QTextOption (Qt::AlignLeft | Qt::AlignVCenter));

        painter.save ();

        QLinearGradient grad (QPointF (0, 0), QPointF (1.0, 1.0));
        grad.setCoordinateMode (QGradient::ObjectBoundingMode);
        grad.setColorAt (0.0, QColor (248, 248, 248));
        grad.setColorAt (1.0, QColor (224, 224, 224));
        painter.setBrush (QBrush (grad));
        painter.setPen (QPen (Qt::black));
        painter.drawRect (QRect (QPoint (x - 2, y - 2), QSize (m_pixmap_size + 3, m_pixmap_size + 3)));

        painter.setBackgroundMode (Qt::TransparentMode);
        painter.setPen (QColor (128, 255, 128));
        painter.drawPixmap (x, y, QBitmap::fromImage (m_green_images [l][t]));
        painter.setPen (QColor (255, 128, 128));
        painter.drawPixmap (x, y, QBitmap::fromImage (m_red_images [l][t]));
        painter.setPen (QColor (128, 128, 255));
        painter.drawPixmap (x, y, QBitmap::fromImage (m_blue_images [l][t]));
        painter.setPen (QColor (255, 255, 128));
        painter.drawPixmap (x, y, QBitmap::fromImage (m_yellow_images [l][t]));
        painter.restore ();

      }

      if (l == 0 && int (m_tolerance_labels.size ()) > visible_columns) {

        int x = x0 + m_first_column_width + m_spacing + visible_columns * (m_column_width + m_spacing);
        int y = m_font_height + m_spacing;

        painter.drawText (QRect (QPoint (x - m_column_width, y), QSize (m_column_width, m_line_height)),
                          QString::fromUtf8 ("..."),
                          QTextOption (Qt::AlignRight | Qt::AlignVCenter));

      }

    }

    if (m_ellipsis) {
      painter.drawText (QRect (QPoint (x0, m_font_height + m_spacing + int (m_layer_labels.size ()) * (m_line_height + m_spacing)), QSize (m_first_column_width, m_font_height)),
                        QString::fromUtf8 ("..."),
                        QTextOption (Qt::AlignLeft | Qt::AlignTop));
    }

  }

private:
  int m_pixmap_size;
  int m_line_height;
  int m_font_height;
  int m_max_lines;
  int m_spacing;
  int m_column_width;
  int m_first_column_width;
  QStringList m_tolerance_labels;
  QStringList m_layer_labels;
  std::vector<QStringList> m_labels;
  std::vector<std::vector<QImage> > m_green_images;
  std::vector<std::vector<QImage> > m_red_images;
  std::vector<std::vector<QImage> > m_yellow_images;
  std::vector<std::vector<QImage> > m_blue_images;
  bool m_ellipsis;
};

// --------------------------------------------------------------------------------------------------
//  XORProgress implementation

XORProgress::XORProgress (const std::string &title, size_t max_count, size_t yield_interval)
  : tl::RelativeProgress (title, max_count, yield_interval), m_needs_update (true), m_dbu (1.0), m_nx (0), m_ny (0)
{
  //  .. nothing yet ..
}

QWidget *XORProgress::progress_widget () const
{
  return new XORProgressWidget ();
}

void XORProgress::render_progress (QWidget *widget) const
{
  XORProgressWidget *pw = dynamic_cast<XORProgressWidget *> (widget);
  if (pw) {
    pw->set_results (m_dbu, m_nx, m_ny, m_results, m_count_per_layer, m_tolerances);
  }
}

void XORProgress::set_results (double dbu, int nx, int ny, const std::map<std::pair<size_t, size_t>, std::map<std::pair<db::LayerProperties, db::Coord>, size_t> > &results, const std::map<db::LayerProperties, size_t> &count_per_layer, const std::vector<db::Coord> &tol)
{
  if (m_count_per_layer != count_per_layer) {
    m_dbu = dbu;
    m_nx = nx;
    m_ny = ny;
    m_results = results;
    m_tolerances = tol;
    m_count_per_layer = count_per_layer;
    m_needs_update = true;
  }
}

}