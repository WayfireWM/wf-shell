#include <wordexp.h>
#include <glibmm/main.h>
#include <gtkmm.h>
#include <gdkmm.h>
#include <gdk/wayland/gdkwayland.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <random>
#include <algorithm>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <gtk4-layer-shell.h>
#include <glib-unix.h>

#include "background.hpp"



static const char *vertex_shader =
    R"(
attribute vec2 in_position;

//uniform mat4 matrix;
attribute highp vec2 uvpos;
varying vec2 uv;

void main() {
    uv = uvpos;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
)";

static const char *fragment_shader =
    R"(
precision highp float;
uniform sampler2D bg_texture_from;
uniform sampler2D bg_texture_to;
uniform float progress;
uniform vec4 from_adj;
uniform vec4 to_adj;

varying vec2 uv;

void main() {
    vec2 from_uv = vec2((uv.x * from_adj.z) - from_adj.x, (uv.y * from_adj.w) - from_adj.y);
    vec2 to_uv = vec2((uv.x * to_adj.z) - to_adj.x, (uv.y * to_adj.w) - to_adj.y);
    vec4 from = texture2D(bg_texture_from, from_uv);
    vec4 to = texture2D(bg_texture_to, to_uv);
    vec3 color = mix(from.rgb, to.rgb, progress);
    gl_FragColor = vec4(color, 1.0);
}
)";

/* Create and compile a shader */
static GLuint create_shader(int type, const char *src)
{
    GLuint shader;
    int status;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        int log_len;
        char *buffer;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);

        buffer = (char*)g_malloc(log_len + 1);
        glGetShaderInfoLog(shader, log_len, NULL, buffer);

        printf("Compile failure in %s shader:\n%s",
            type == GL_VERTEX_SHADER ? "vertex" : "fragment",
            buffer);

        g_free(buffer);

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

/* Initialize the shaders and link them into a program */
static GLuint init_shaders()
{
    GLuint vertex, fragment;
    GLuint program = 0;
    int status;

    vertex = create_shader(GL_VERTEX_SHADER, vertex_shader);

    if (vertex == 0)
    {
        return 0;
    }

    fragment = create_shader(GL_FRAGMENT_SHADER, fragment_shader);

    if (fragment == 0)
    {
        glDeleteShader(vertex);
        return 0;
    }

    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        int log_len;
        char *buffer;

        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);

        buffer = (char*)g_malloc(log_len + 1);
        glGetProgramInfoLog(program, log_len, NULL, buffer);

        g_warning("Linking failure:\n%s", buffer);

        g_free(buffer);

        glDeleteProgram(program);
        program = 0;

        goto out;
    }

    glDetachShader(program, vertex);
    glDetachShader(program, fragment);

out:
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

void BackgroundImage::generate_adjustments(int width, int height)
{
    // Sanity checks
    if ((width == 0) ||
        (height == 0) ||
        (source == nullptr) ||
        (source->get_width() == 0) ||
        (source->get_height() == 0))
    {
        return;
    }

    double screen_width  = (double)width;
    double screen_height = (double)height;
    double source_width  = (double)source->get_width();
    double source_height = (double)source->get_height();

    adjustments = Glib::RefPtr<BackgroundImageAdjustments>(new BackgroundImageAdjustments());
    std::string fill_and_crop_string = "fill_and_crop";
    std::string stretch_string = "stretch";
    if (!stretch_string.compare(fill_type))
    {
        adjustments->x = 0.0;
        adjustments->y = 0.0;
        adjustments->scale_x = 1.0;
        adjustments->scale_y = 1.0;
    } else if (!fill_and_crop_string.compare(fill_type))
    {
        auto width_difference  = screen_width - source_width;
        auto height_difference = screen_height - source_height;
        if (width_difference > height_difference)
        {
            adjustments->scale_x = 1.0;
            adjustments->scale_y = (screen_height / screen_width) * (source_width / source_height);
            adjustments->x = 0.0;
            adjustments->y = (screen_height - source_height * (screen_width / source_width)) *
                adjustments->scale_y * 0.5 * (1.0 / screen_height);
        } else
        {
            adjustments->scale_x = (screen_width / screen_height) * (source_height / source_width);
            adjustments->scale_y = 1.0;
            adjustments->x = (screen_width - source_width * (screen_height / source_height)) *
                adjustments->scale_x * 0.5 * (1.0 / screen_width);
            adjustments->y = 0.0;
        }
    } else /* "preserve_aspect" */
    {
        auto width_difference  = screen_width / source_width;
        auto height_difference = screen_height / source_height;
        if (width_difference > height_difference)
        {
            adjustments->scale_x = (screen_width / screen_height) * (source_height / source_width);
            adjustments->scale_y = 1.0;
            adjustments->x = (screen_width - source_width * (screen_height / source_height)) *
                adjustments->scale_x * 0.5 * (1.0 / screen_width);
            adjustments->y = 0.0;
        } else
        {
            adjustments->scale_x = 1.0;
            adjustments->scale_y = (screen_height / screen_width) * (source_width / source_height);
            adjustments->x = 0.0;
            adjustments->y = (screen_height - source_height * (screen_width / source_width)) *
                adjustments->scale_y * 0.5 * (1.0 / screen_height);
        }
    }
}

void BackgroundGLArea::show_image(Glib::RefPtr<BackgroundImage> next_image)
{
    if (!next_image || !next_image->source ||
        (background->window_width <= 0) || (background->window_height <= 0))
    {
        to_image   = nullptr;
        from_image = nullptr;
        return;
    }

    from_image = to_image;
    to_image   = next_image;

    int width, height;

    to_image->generate_adjustments(background->window_width, background->window_height);
    width  = to_image->source->get_width();
    height = to_image->source->get_height();
    glBindTexture(GL_TEXTURE_2D, to_image->tex_id);
    auto format = to_image->source->get_has_alpha() ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE,
        to_image->source->get_pixels());
    glBindTexture(GL_TEXTURE_2D, 0);

    fade = {
        fade_duration,
        wf::animation::smoothing::sigmoid
    };
    fade.animate(0.0, 1.0);

    this->queue_draw();
}

static GLuint create_texture()
{
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

BackgroundImage::BackgroundImage()
{
    tex_id = create_texture();
}

BackgroundImage::~BackgroundImage()
{
    glDeleteTextures(1, &tex_id);
}

Glib::RefPtr<BackgroundImage> WayfireBackground::create_from_file_safe(std::string path)
{
    Glib::RefPtr<BackgroundImage> image = Glib::RefPtr<BackgroundImage>(new BackgroundImage());
    image->fill_type = (std::string)background_fill_mode;

    try {
        image->source = Gdk::Pixbuf::create_from_file(path);
    } catch (...)
    {
        return nullptr;
    }

    if (image->source == nullptr)
    {
        return nullptr;
    }

    return image;
}

bool WayfireBackground::change_background()
{
    auto next_image = load_next_background();
    if (!next_image)
    {
        return false;
    }

    gl_area->show_image(next_image);

    return true;
}

bool WayfireBackground::load_images_from_dir(std::string path)
{
    wordexp_t exp;

    /* Expand path */
    if (wordexp(path.c_str(), &exp, 0))
    {
        return false;
    }

    if (!exp.we_wordc)
    {
        wordfree(&exp);
        return false;
    }

    auto dir = opendir(exp.we_wordv[0]);
    if (!dir)
    {
        wordfree(&exp);
        return false;
    }

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
        {
            continue;
        }

        auto fullpath = std::string(exp.we_wordv[0]) + "/" + file->d_name;

        struct stat next;
        if (stat(fullpath.c_str(), &next) == 0)
        {
            if (S_ISDIR(next.st_mode))
            {
                /* Recursive search */
                load_images_from_dir(fullpath);
            } else
            {
                images.push_back(fullpath);
            }
        }
    }

    wordfree(&exp);

    if (background_randomize && images.size())
    {
        std::random_device random_device;
        std::mt19937 random_gen(random_device());
        std::shuffle(images.begin(), images.end(), random_gen);
    }

    return true;
}

Glib::RefPtr<BackgroundImage> WayfireBackground::load_next_background()
{
    Glib::RefPtr<BackgroundImage> image;
    while (!image)
    {
        if (!images.size())
        {
            std::cerr << "Failed to load background images from " <<
                    (std::string)background_image << std::endl;
            // window.remove();
            return nullptr;
        }

        current_background = (current_background + 1) % images.size();

        auto path = images[current_background];
        image = create_from_file_safe(path);

        if (!image)
        {
            images.erase(images.begin() + current_background);
        } else
        {
            std::cout << "Picked background " << path << std::endl;
        }
    }

    return image;
}

void WayfireBackground::update_background()
{
    Glib::RefPtr<BackgroundImage> image = Glib::RefPtr<BackgroundImage>(new BackgroundImage());
    auto current = gl_area->get_current_image();
    if (current != nullptr)
    {
        image->source    = current->source;
        image->fill_type = background_fill_mode;
        gl_area->show_image(image);
    }
}

void WayfireBackground::reset_background()
{
    images.clear();
    current_background = 0;
    change_bg_conn.disconnect();
}

void WayfireBackground::set_background()
{
    reset_background();

    std::string path = background_image;
    try {
        if (load_images_from_dir(path) && images.size())
        {
            auto image = load_next_background();
            if (!image)
            {
                throw std::exception();
            }

            gl_area->show_image(image);
        } else
        {
            auto image = create_from_file_safe(path);
            if (!image)
            {
                throw std::exception();
            }

            gl_area->show_image(image);
        }
    } catch (...)
    {
        std::cerr << "Failed to load background image(s) " << path << std::endl;
    }

    reset_cycle_timeout();

    if (inhibited && output->output)
    {
        zwf_output_v2_inhibit_output_done(output->output);
        inhibited = false;
    }
}

void WayfireBackground::reset_cycle_timeout()
{
    int cycle_timeout = background_cycle_timeout * 1000;
    change_bg_conn.disconnect();
    if (images.size())
    {
        change_bg_conn = Glib::signal_timeout().connect(sigc::mem_fun(
            *this, &WayfireBackground::change_background), cycle_timeout);
    }
}

void BackgroundGLArea::realize()
{
    this->make_current();
    program = init_shaders();
}

bool BackgroundGLArea::render(const Glib::RefPtr<Gdk::GLContext>& context)
{
    static const float vertices[] = {
        1.0f, 1.0f,
        -1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f, -1.0f,
    };
    static const float uv_coords[] = {
        1.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };
    static float from_adj[4] = {0.0, 0.0, 1.0, 1.0};
    if (from_image)
    {
        from_adj[0] = from_image->adjustments->x;
        from_adj[1] = from_image->adjustments->y;
        from_adj[2] = from_image->adjustments->scale_x;
        from_adj[3] = from_image->adjustments->scale_y;
    }

    static float to_adj[4] = {0.0, 0.0, 1.0, 1.0};
    if (to_image)
    {
        to_adj[0] = to_image->adjustments->x;
        to_adj[1] = to_image->adjustments->y;
        to_adj[2] = to_image->adjustments->scale_x;
        to_adj[3] = to_image->adjustments->scale_y;
    }

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    if (from_image)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, from_image->tex_id);
        GLuint from_tex_uniform = glGetUniformLocation(program, "bg_texture_from");
        glUniform1i(from_tex_uniform, 0);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, to_image->tex_id);
    GLuint to_tex_uniform = glGetUniformLocation(program, "bg_texture_to");
    glUniform1i(to_tex_uniform, 1);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv_coords);
    GLuint progress = glGetUniformLocation(program, "progress");
    glUniform1f(progress, fade);
    GLuint from_adj_loc = glGetUniformLocation(program, "from_adj");
    glUniform4fv(from_adj_loc, 1, from_adj);
    GLuint to_adj_loc = glGetUniformLocation(program, "to_adj");
    glUniform4fv(to_adj_loc, 1, to_adj);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);

    if (fade.running())
    {
        Glib::signal_idle().connect([=] ()
        {
            this->queue_draw();
            return false;
        });
    }

    return true;
}

BackgroundGLArea::BackgroundGLArea(WayfireBackground *background)
{
    this->background = background;
}

BackgroundWindow::BackgroundWindow(WayfireBackground *background)
{
    this->background = background;
}

void BackgroundWindow::size_allocate_vfunc(int width, int height, int baseline)
{
    Gtk::Widget::size_allocate_vfunc(width, height, baseline);
    background->window_width  = width;
    background->window_height = height;

    background->set_background();
}

void WayfireBackground::setup_window()
{
    gl_area = Glib::RefPtr<BackgroundGLArea>(new BackgroundGLArea(this));
    window  = Glib::RefPtr<BackgroundWindow>(new BackgroundWindow(this));
    window->set_decorated(false);

    gtk_layer_init_for_window(window->gobj());
    gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_monitor(window->gobj(), this->output->monitor->gobj());

    gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
    gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
    gtk_layer_set_keyboard_mode(window->gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    gtk_layer_set_exclusive_zone(window->gobj(), -1);
    gl_area->signal_realize().connect(sigc::mem_fun(*gl_area, &BackgroundGLArea::realize));
    gl_area->signal_render().connect(sigc::mem_fun(*gl_area, &BackgroundGLArea::render), false);
    window->set_child(*gl_area);

    auto update_background = [=] () { this->update_background(); };
    auto set_background    = [=] () { this->set_background(); };
    auto reset_cycle = [=] () { reset_cycle_timeout(); };
    background_image.set_callback(set_background);
    background_fill_mode.set_callback(update_background);
    background_cycle_timeout.set_callback(reset_cycle);

    window->present();
}

WayfireBackground::WayfireBackground(WayfireShellApp *app, WayfireOutput *output)
{
    this->app    = app;
    this->output = output;

    if (output->output)
    {
        this->inhibited = true;
        zwf_output_v2_inhibit_output(output->output);
    }

    setup_window();
}

WayfireBackground::~WayfireBackground()
{
    reset_background();
}

class WayfireBackgroundApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfireBackground>> backgrounds;

  public:
    using WayfireShellApp::WayfireShellApp;
    static void create(int argc, char **argv)
    {
        WayfireShellApp::instance =
            std::make_unique<WayfireBackgroundApp>();
        g_unix_signal_add(SIGUSR1, sigusr1_handler, (void*)instance.get());
        instance->run(argc, argv);
    }

    void handle_new_output(WayfireOutput *output) override
    {
        backgrounds[output] = std::unique_ptr<WayfireBackground>(
            new WayfireBackground(this, output));
    }

    void handle_output_removed(WayfireOutput *output) override
    {
        backgrounds.erase(output);
    }

    static gboolean sigusr1_handler(void *instance)
    {
        for (const auto& [_, bg] : ((WayfireBackgroundApp*)instance)->backgrounds)
        {
            bg->change_background();
        }

        return TRUE;
    }
};

int main(int argc, char **argv)
{
    WayfireBackgroundApp::create(argc, argv);
    return 0;
}
