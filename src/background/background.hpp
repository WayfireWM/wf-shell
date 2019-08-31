#pragma once

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
    void show_image(Glib::RefPtr<Gdk::Pixbuf> image, double offset_x, double offset_y);

    protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
};

class WayfireBackground
{
    WayfireShellApp *app;
    WayfireOutput *output;
    zwf_wm_surface_v1 *wm_surface = NULL;

    BackgroundDrawingArea drawing_area;
    std::vector<std::string> images;
    Gtk::Window window;
    int scale;
    double offset_x, offset_y;
    bool inhibited = true;
    uint current_background;
    int output_width, output_height;
    sigc::connection change_bg_conn;

    wf_option background_image, background_color, background_cycle_timeout,
        background_randomize, background_preserve_aspect;
    wf_option_callback init_background, background_color_changed, cycle_timeout_updated;

    Glib::RefPtr<Gdk::Pixbuf> create_from_file_safe(std::string path);
    Glib::RefPtr<Gtk::CssProvider> current_css_provider;
    void create_wm_surface();
    void handle_output_resize(uint32_t width, uint32_t height);
    void set_background_color();
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
