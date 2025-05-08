#pragma once

#include <glibmm/refptr.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <wf-shell-app.hpp>
#include <wf-option-wrap.hpp>
#include <wayfire/util/duration.hpp>

#include <epoxy/gl.h>

class WayfireBackground;

class BackgroundImageAdjustments
{
  public:
    GLfloat scale_x = -1, scale_y = -1;
    GLfloat x, y;
};

class BackgroundImage
{
  public:
    Glib::RefPtr<Gdk::Pixbuf> source;
    std::string fill_type;
    Glib::RefPtr<BackgroundImageAdjustments> adjustments;

    void generate_adjustments(int width, int height);
};

class BackgroundGLArea : public Gtk::GLArea
{
    WayfireBackground *background;
    GLuint program = 0;
    wf::animation::simple_animation_t fade;
    WfOption<int> fade_duration{"background/fade_duration"};

    /* These two pixbufs are used for fading one background
     * image to the next when changing backgrounds or when
     * automatically cycling through a directory of images.
     * pbuf is the current image to which we are fading and
     * pbuf2 is the image from which we are fading. x and y
     * are used as offsets when preserve aspect is set. */
    Glib::RefPtr<BackgroundImage> to_image, from_image;
    GLuint from_tex = 0;
    GLuint to_tex = 0;
  public:
    BackgroundGLArea(WayfireBackground *background);
    void realize();
    bool render(const Glib::RefPtr<Gdk::GLContext>& context);
    void show_image(Glib::RefPtr<BackgroundImage> image);
    Glib::RefPtr<BackgroundImage> get_current_image()
    {
        return to_image;
    }
};

class BackgroundWindow : public Gtk::Window
{
    WayfireBackground *background;
  public:
    BackgroundWindow(WayfireBackground *background);
  protected:
    void size_allocate_vfunc(int width, int height, int baseline) override;
};

class WayfireBackground
{
    WayfireShellApp *app;
    WayfireOutput *output;

    Glib::RefPtr<BackgroundGLArea> gl_area;
    std::vector<std::string> images;
    Glib::RefPtr<Gtk::Window> window;

    bool inhibited = false;
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
    void update_background();
    void reset_cycle_timeout();

    void setup_window();

  public:
    guint window_width = 0;
    guint window_height = 0;
    WayfireBackground(WayfireShellApp *app, WayfireOutput *output);
    void set_background();
    bool change_background();
    ~WayfireBackground();
};
