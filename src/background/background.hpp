#pragma once

#include <glibmm/refptr.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <wf-shell-app.hpp>
#include <wf-option-wrap.hpp>
#include <wayfire/util/duration.hpp>

class WayfireBackground;

class BackgroundImage
{
  public:
    double scale;
    double x, y;
    Cairo::RefPtr<Cairo::Surface> source;
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
    BackgroundImage to_image, from_image;

  public:
    BackgroundDrawingArea();
    void show_image(Glib::RefPtr<Gdk::Pixbuf> image,
        double offset_x, double offset_y, double image_scale);

  protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
};

class WayfireBackground
{
    WayfireShellApp *app;
    WayfireOutput *output;

    BackgroundDrawingArea drawing_area;
    std::vector<std::string> images;
    Gtk::Window window;

    int scale;
    double offset_x, offset_y;
    double image_scale = 1.0;
    bool inhibited     = false;
    uint current_background;
    sigc::connection change_bg_conn;

    WfOption<std::string> background_image{"background/image"};
    WfOption<int> background_cycle_timeout{"background/cycle_timeout"};
    WfOption<bool> background_randomize{"background/randomize"};
    WfOption<std::string> background_fill_mode{"background/fill_mode"};

    Glib::RefPtr<Gdk::Pixbuf> create_from_file_safe(std::string path);
    bool background_transition_frame(int timer);
    bool load_images_from_dir(std::string path);
    bool load_next_background(Glib::RefPtr<Gdk::Pixbuf> & pbuf, std::string & path);
    void reset_background();
    void set_background();
    void reset_cycle_timeout();

    void setup_window();

  public:
    WayfireBackground(WayfireShellApp *app, WayfireOutput *output);
    bool change_background();
    ~WayfireBackground();
};
