#pragma once

#include <glibmm/refptr.h>
#include <config.hpp>
#include <animation.hpp>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <wf-shell-app.hpp>

class WayfireBackground;

class BackgroundImage
{
    public:
    double x, y;
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
};

class BackgroundDrawingArea : public Gtk::DrawingArea
{
    wf_duration fade;
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
        double offset_x, double offset_y);

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
    bool inhibited = false;
    uint current_background;
    sigc::connection change_bg_conn;

    wf_option background_image, background_cycle_timeout,
        background_randomize, background_preserve_aspect;
    wf_option_callback init_background, cycle_timeout_updated;

    Glib::RefPtr<Gdk::Pixbuf> create_from_file_safe(std::string path);
    bool background_transition_frame(int timer);
    bool change_background(int timer);
    bool load_images_from_dir(std::string path);
    bool load_next_background(Glib::RefPtr<Gdk::Pixbuf> &pbuf, std::string &path);
    void reset_background();
    void set_background();
    void reset_cycle_timeout();

    void setup_window();

  public:
    WayfireBackground(WayfireShellApp *app, WayfireOutput *output);
    ~WayfireBackground();
};
