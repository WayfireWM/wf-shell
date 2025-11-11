#ifndef WF_PANEL_POPOVER_HPP
#define WF_PANEL_POPOVER_HPP

#include <sigc++/signal.h>   // Only signal.h to avoid incomplete type issues
#include <gtkmm/menubutton.h>
#include <gtkmm/popover.h>
#include <wf-option-wrap.hpp>

/**
 * A button which shows a popover on click. It adjusts the popup position
 * automatically based on panel position (valid values are "top" and "bottom")
 */
class WayfireMenuButton : public Gtk::MenuButton
{
    bool interactive = true;
    bool has_focus   = false;
    WfOption<std::string> panel_position;

    void set_active_on_window();
    friend class WayfireAutohidingWindow;
    void set_has_focus(bool focus);

  public:
    Gtk::Popover m_popover;

    WayfireMenuButton(const std::string& config_section);
    virtual ~WayfireMenuButton() = default;

    void set_keyboard_interactive(bool interactive = true);
    bool is_keyboard_interactive() const;
    bool is_popover_focused() const;
    void grab_focus();

    sigc::signal<void()>& signal_clicked() { return m_signal_clicked; }

  private:
    sigc::signal<void()> m_signal_clicked;

    void on_realize() override
    {
        Gtk::MenuButton::on_realize();

        // Emit clicked when the popover is shown
        m_popover.signal_show().connect([this]() { m_signal_clicked.emit(); });
    }
};

#endif /* WF_PANEL_POPOVER_HPP */
