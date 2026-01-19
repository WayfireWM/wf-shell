#ifndef WF_LOCKER_GRID
#define WF_LOCKER_GRID
#include "gtkmm/enums.h"
#include <exception>
#include <gtkmm/centerbox.h>
#include <gtkmm/box.h>
#include <gtkmm/widget.h>

class WayfireLockerGrid : public Gtk::CenterBox
{
    Gtk::CenterBox row1, row2, row3;
    Gtk::Box box[9];
    
  public:
    /* Config string to box from grid */
    void attach(Gtk::Widget & widget, std::string pos_string)
    {
        if (pos_string == "top-left")
        {
            attach(widget, 0, 0);
        } else if (pos_string == "top-center")
        {
            attach(widget, 1, 0);
        } else if (pos_string == "top-right")
        {
            attach(widget, 2, 0);
        } else if (pos_string == "center-left")
        {
            attach(widget, 0, 1);
        } else if (pos_string == "center-center")
        {
            attach(widget, 1, 1);
        } else if (pos_string == "center-right")
        {
            attach(widget, 2, 1);
        } else if (pos_string == "bottom-left")
        {
            attach(widget, 0, 2);
        } else if (pos_string == "bottom-center")
        {
            attach(widget, 1, 2);
        } else if (pos_string == "bottom-right")
        {
            attach(widget, 2, 2);
        } else
        {
            throw std::exception();
        }
    }

    void attach(Gtk::Widget & widget, int col, int row)
    {
        if ((col > 2) || (row > 2) || (col < 0) || (row < 0))
        {
            throw std::exception();
        }
        
        box[col + (row * 3)].append(widget);
    }

    WayfireLockerGrid()
    {
        set_start_widget(row1);
        set_center_widget(row2);
        set_end_widget(row3);
        set_orientation(Gtk::Orientation::VERTICAL);
        row1.set_start_widget(box[0]);
        row1.set_center_widget(box[1]);
        row1.set_end_widget(box[2]);
        row2.set_start_widget(box[3]);
        row2.set_center_widget(box[4]);
        row2.set_end_widget(box[5]);
        row3.set_start_widget(box[6]);
        row3.set_center_widget(box[7]);
        row3.set_end_widget(box[8]);
        for(int i = 0; i < 9; i ++)
        {
            box[i].set_orientation(Gtk::Orientation::VERTICAL);
        }
    }
};

#endif