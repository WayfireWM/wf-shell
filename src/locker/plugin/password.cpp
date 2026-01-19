#include <iostream>
#include <memory>
#include <glibmm.h>
#include "gtkmm/entry.h"
#include "gtkmm/label.h"
#include <unistd.h>
#include <security/_pam_types.h>
#include <security/pam_appl.h>

#include "locker.hpp"
#include "lockergrid.hpp"
#include "password.hpp"

bool WayfireLockerPasswordPlugin::should_enable()
{
    return (bool)enable;
}

void WayfireLockerPasswordPlugin::update_labels(std::string text)
{
    for (auto& it : labels)
    {
        it.second->set_label(text);
    }

    label_contents = text;
}

void WayfireLockerPasswordPlugin::blank_passwords()
{
    for (auto& it : entries)
    {
        it.second->set_text("");
    }
}

void WayfireLockerPasswordPlugin::add_output(int id, WayfireLockerGrid *grid)
{
    labels.emplace(id, std::shared_ptr<Gtk::Label>(new Gtk::Label()));
    entries.emplace(id, std::shared_ptr<Gtk::Entry>(new Gtk::Entry));
    auto label = labels[id];
    auto entry = entries[id];
    label->add_css_class("password-reply");
    entry->add_css_class("password-entry");
    entry->set_placeholder_text("Password");
    label->set_label(label_contents);
    entry->set_visibility(false);
    /* Set entry callback for return */
    entry->signal_activate().connect([this, entry] ()
    {
        auto password = entry->get_text();
        if (password.length() > 0)
        {
            submit_user_password(password);
        }
    }, true);
    /* Add to window */
    grid->attach(*entry, WfOption<std::string>{"locker/password_position"});
    grid->attach(*label, WfOption<std::string>{"locker/password_position"});
}

void WayfireLockerPasswordPlugin::remove_output(int id)
{
    labels.erase(id);
    entries.erase(id);
}

WayfireLockerPasswordPlugin::WayfireLockerPasswordPlugin()
{}

/* PAM password C code... */
int pam_conversation(int num_mesg, const struct pam_message **mesg, struct pam_response **resp,
    void *appdata_ptr)
{
    std::cout << "PAM convo step ... " << std::endl;

    WayfireLockerPasswordPlugin *pass_plugin = (WayfireLockerPasswordPlugin*)appdata_ptr;
    *resp = (struct pam_response*)calloc(num_mesg, sizeof(struct pam_response));
    if (*resp == NULL)
    {
        std::cerr << "PAM reply allocation failed" << std::endl;
        return PAM_ABORT;
    }

    for (int count = 0; count < num_mesg; count++)
    {
        std::cout << "PAM msg : " << mesg[count]->msg << std::endl;
        resp[count]->resp_retcode = 0;
        /* Echo OFF prompt should be user password. */
        if (mesg[count]->msg_style == PAM_PROMPT_ECHO_OFF)
        {
            resp[count]->resp = strdup(pass_plugin->submitted_password.c_str());
        } else if (mesg[count]->msg_style == PAM_ERROR_MSG)
        {
            pass_plugin->update_labels(mesg[count]->msg);
        } else if (mesg[count]->msg_style == PAM_TEXT_INFO)
        {
            pass_plugin->update_labels(mesg[count]->msg);
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
    retval = pam_start("wf-locker-password", username, &local_conversation, &local_auth_handle);
    if (retval != PAM_SUCCESS)
    {
        /* We don't expect to be here. No graceful way out of this. */
        std::cout << "PAM start returned " << retval << std::endl;
        update_labels("pam_start failure");
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
            std::cout << "Authentication failure." << std::endl;
            update_labels("Authentication failure.");
        }
    } else
    {
        std::cout << "Authenticate success." << std::endl;
        unlock = true;
    }

    retval = pam_end(local_auth_handle, retval);
    if (unlock)
    {
        WayfireLockerApp::get().unlock();
    }
}

void WayfireLockerPasswordPlugin::init()
{}
