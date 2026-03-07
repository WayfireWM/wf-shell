#include <iostream>
#include <memory>
#include <glibmm.h>
#include "gtkmm/entry.h"
#include "gtkmm/enums.h"
#include "gtkmm/label.h"
#include <unistd.h>
#include <security/_pam_types.h>
#include <security/pam_appl.h>

#include "locker.hpp"
#include "lockergrid.hpp"
#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "password.hpp"

WayfireLockerPasswordPluginWidget::~WayfireLockerPasswordPluginWidget()
{
    if (entry_updated)
    {
        entry_updated.disconnect();
    }

    if (entry_submitted)
    {
        entry_submitted.disconnect();
    }

    for (auto signal : replies_signals)
    {
        signal.disconnect();
    }
}

void WayfireLockerPasswordPlugin::add_reply(std::string text)
{
    for (auto& it : widgets)
    {
        it.second->add_reply(text);
    }
}

void WayfireLockerPasswordPlugin::blank_passwords()
{
    for (auto& it : widgets)
    {
        it.second->entry.set_text("");
    }
}

void WayfireLockerPasswordPluginWidget::lockout_changed(bool lockout)
{
    if (lockout)
    {
        entry.set_text("");
        entry.set_sensitive(false);
    } else
    {
        entry.set_sensitive(true);
    }
}

void WayfireLockerPasswordPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerPasswordPluginWidget());

    /* Share string to every other entry */
    auto widget = widgets[id];
    widget->entry_updated = widget->entry.signal_changed().connect(
        [this, widget] ()
    {
        int start, end;
        widget->entry.get_selection_bounds(start, end);
        auto password = widget->entry.get_text();
        for (auto & pair : widgets)
        {
            pair.second->entry_updated.block(true);
            pair.second->entry.set_text(password);
            pair.second->entry.set_position(start);
            pair.second->entry_updated.unblock();
        }
    }, true);

    /* Set entry callback for return */
    widget->entry_submitted = widget->entry.signal_activate().connect([this, widget] ()
    {
        auto password = widget->entry.get_text();
        if (password.length() > 0)
        {
            submit_user_password(password);
        }
    }, true);
    /* Add to window */
    grid->attach(*widget, position);

    widget->signal_realize().connect([=] ()
    {
        widget->entry.grab_focus();
    });
}

WayfireLockerPasswordPluginWidget::WayfireLockerPasswordPluginWidget() :
    WayfireLockerTimedRevealer("locker/password_always")
{
    set_child(box);
    box.add_css_class("password");
    box.append(entry);
    box.append(replies);
    box.set_orientation(Gtk::Orientation::VERTICAL);
    replies.add_css_class("password-reply");
    replies.set_orientation(Gtk::Orientation::VERTICAL);
    entry.add_css_class("password-entry");
    entry.set_placeholder_text("Password");
    entry.set_visibility(false);
}

void WayfireLockerPasswordPluginWidget::add_reply(std::string message)
{
    std::shared_ptr<Gtk::Label> new_label = std::make_shared<Gtk::Label>(message);
    replies.append(*new_label);

    replies_signals.push_back(Glib::signal_timeout().connect_seconds([this, new_label] ()
    {
        replies.remove(*new_label);
        return G_SOURCE_REMOVE;
    }, 15));
}

void WayfireLockerPasswordPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}

WayfireLockerPasswordPlugin::WayfireLockerPasswordPlugin() :
    WayfireLockerPlugin("locker/password")
{}

/* PAM password C code... */
int pam_conversation(int num_mesg, const struct pam_message **mesg, struct pam_response **resp,
    void *appdata_ptr)
{
    WayfireLockerPasswordPlugin *pass_plugin = (WayfireLockerPasswordPlugin*)appdata_ptr;
    *resp = (struct pam_response*)calloc(num_mesg, sizeof(struct pam_response));
    if (*resp == NULL)
    {
        std::cerr << "PAM reply allocation failed" << std::endl;
        return PAM_ABORT;
    }

    for (int count = 0; count < num_mesg; count++)
    {
        std::string message = mesg[count]->msg;

        resp[count]->resp_retcode = 0;
        /* Echo OFF prompt should be user password. */
        if (mesg[count]->msg_style == PAM_PROMPT_ECHO_OFF)
        {
            resp[count]->resp = strdup(pass_plugin->submitted_password.c_str());
        } else if (mesg[count]->msg_style == PAM_ERROR_MSG)
        {
            pass_plugin->add_reply(message);
        } else if (mesg[count]->msg_style == PAM_TEXT_INFO)
        {
            pass_plugin->add_reply(message);
        }
    }

    return PAM_SUCCESS;
}

void WayfireLockerPasswordPlugin::submit_user_password(std::string password)
{
    submitted_password = password;
    blank_passwords();
    std::cout << "Unlocking ... " << std::endl;
    /* Get username*/
    char *username = getlogin();
    /* Init PAM conversation */
    const struct pam_conv local_conversation = {
        pam_conversation, this
    };
    pam_handle_t *local_auth_handle = NULL; // this gets set by pam_start
    int retval;
    /* Start the password-based conversation */
    std::cout << "PAM start ... " << std::endl;
    retval = pam_start("wf-locker", username, &local_conversation, &local_auth_handle);
    if (retval != PAM_SUCCESS)
    {
        /* We don't expect to be here. No graceful way out of this. */
        add_reply("pam_start failure");
        exit(retval);
    }

    std::cout << "PAM auth ... " << std::endl;
    /* Request authenticate */
    retval = pam_authenticate(local_auth_handle, 0);
    bool unlock = false;
    if (retval != PAM_SUCCESS)
    {
        if (retval == PAM_AUTH_ERR)
        {
            failure();
        }
    } else
    {
        std::cout << "Authenticate success." << std::endl;
        unlock = true;
    }

    retval = pam_end(local_auth_handle, retval);
    if (unlock)
    {
        WayfireLockerApp::get().perform_unlock("PAM Password authenticated");
    }
}

void WayfireLockerPasswordPlugin::lockout_changed(bool lockout)
{
    for (auto & it : widgets)
    {
        it.second->lockout_changed(lockout);
    }
}

void WayfireLockerPasswordPlugin::failure()
{
    WayfireLockerApp::get().recieved_bad_auth();
    for (auto & it : widgets)
    {
        it.second->failure();
    }
}

void WayfireLockerPasswordPlugin::init()
{}

void WayfireLockerPasswordPlugin::deinit()
{}
