#pragma once
#include <glibmm.h>
#include <epoxy/gl.h>
#include <gtkmm.h>
#include <gdkmm.h>
#include <memory>
#include <wf-option-wrap.hpp>
#include <wayfire/util/duration.hpp>
#include <gdk-pixbuf/gdk-pixbuf.h>


class BackgroundImageAdjustments
{
  public:
    GLfloat scale_x = -1, scale_y = -1;
    GLfloat x, y;
};

class BackgroundImage
{
  public:
    BackgroundImage();
    ~BackgroundImage();
    Glib::RefPtr<Gdk::Pixbuf> source;
    std::string fill_type;
    Glib::RefPtr<BackgroundImageAdjustments> adjustments;
    void generate_adjustments(int width, int height);
    GLuint tex_id = 0;
};

class BackgroundGLArea : public Gtk::GLArea
{
    GLuint program = 0;
    wf::animation::simple_animation_t fade;

    /* These two pixbufs are used for fading one background
     * image to the next when changing backgrounds or when
     * automatically cycling through a directory of images.
     * pbuf is the current image to which we are fading and
     * pbuf2 is the image from which we are fading. x and y
     * are used as offsets when preserve aspect is set. */
    std::shared_ptr<BackgroundImage> to_image, from_image;
    void show_image(std::shared_ptr<BackgroundImage> image);

  public:
    BackgroundGLArea();
    void realize();
    bool render(const Glib::RefPtr<Gdk::GLContext>& context);
    bool show_image(std::string path);

    std::shared_ptr<BackgroundImage> get_current_image()
    {
        return to_image;
    }
};
