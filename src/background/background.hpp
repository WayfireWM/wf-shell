#pragma once

#include <glibmm/refptr.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <wf-shell-app.hpp>
#include <wf-option-wrap.hpp>
#include <wayfire/util/duration.hpp>

class WayfireBackground;

class BackgroundImageAdjustments
{
  public:
  double scale_x=-1, scale_y=-1;
  double x,y;
};

class BackgroundImage
{
  public:
    Glib::RefPtr<Gdk::Pixbuf> source;
    std::string fill_type;

    Glib::RefPtr<BackgroundImageAdjustments> generate_adjustments(int width, int height);
};

class BackgroundDrawingArea : public Gtk::DrawingArea
{
    wf::animation::simple_animation_t fade;
    WfOption<int> fade_duration{"background/fade_duration"};

    /* These two pixbufs are used for fading one background
     * image to the next when changing backgrounds or when
     * automatically cycling through a directory of images.
     * pbuf is the current image to which we are fading and
     * pbuf2 is the image from which we are fading. x and y
     * are used as offsets when preserve aspect is set. */
    Glib::RefPtr<BackgroundImage> to_image, from_image;

  public:
    BackgroundDrawingArea();
    gboolean update_animation(Glib::RefPtr<Gdk::FrameClock> clock);
    void show_image(Glib::RefPtr<BackgroundImage> image);
    bool do_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);
};

class WayfireBackground
{
    WayfireShellApp *app;
    WayfireOutput *output;

    BackgroundDrawingArea drawing_area;
    std::vector<std::string> images;
    Gtk::Window window;

    bool inhibited     = false;
    uint current_background;
    sigc::connection change_bg_conn;

    WfOption<std::string> background_image{"background/image"};
    WfOption<int> background_cycle_timeout{"background/cycle_timeout"};
    WfOption<bool> background_randomize{"background/randomize"};
    WfOption<std::string> background_fill_mode{"background/fill_mode"};

    Glib::RefPtr<BackgroundImage> create_from_file_safe(std::string path);
    bool background_transition_frame(int timer);
    bool load_images_from_dir(std::string path);
    Glib::RefPtr<BackgroundImage> load_next_background();
    void reset_background();
    void set_background();
    void reset_cycle_timeout();

    void setup_window();

  public:
    WayfireBackground(WayfireShellApp *app, WayfireOutput *output);
    bool change_background();
    ~WayfireBackground();
};
