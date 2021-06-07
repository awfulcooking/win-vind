#include "about.hpp"

#include "disable_gcc_warning.hpp"

#include <wx/button.h>
#include <wx/defs.h>
#include <wx/font.h>
#include <wx/fontenum.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "enable_gcc_warning.hpp"

#include "err_logger.hpp"

#include "version.hpp"


namespace vindgui
{
    AboutDialog::AboutDialog(
            const std::string& iconpath,
            const std::string& tooltips,
            int font_size,
            const std::string& font_name)
    : wxDialog(nullptr, wxID_ANY, wxT("About win-vind")),
      systray_(std::make_unique<SystemTray>(this, iconpath, tooltips))
    {
        SetIcon(wxIcon(iconpath, wxBITMAP_TYPE_ICO)) ;

        auto font = wxFont::New(9, wxFONTFAMILY_TELETYPE, wxFONTFLAG_DEFAULT) ;
        font->SetPointSize(font_size) ;
        if(wxFontEnumerator().IsValidFacename(font_name)) {
            font->SetFaceName(font_name) ;
        }
        else {
            PRINT_ERROR("The font name " + font_name + " is not available.") ;
        }

        auto root = new wxBoxSizer(wxVERTICAL) ;

        wxSizerFlags flags ;
        flags.Border(wxALL, 30) ;
        flags.Align(wxALIGN_CENTER_HORIZONTAL) ;

        constexpr auto message = wxT(\
            "Version: " WIN_VIND_VERSION
            "\n"
            "\n"
            "License: MIT License (C) 2020-2021 pit-ray") ;

        auto main_text = new wxStaticText(this, wxID_ANY, message) ;
        main_text->SetFont(*font) ;
        root->Add(main_text, flags) ;

        flags.Border(wxALL, 10) ;
        auto btn = new wxButton(this, wxID_CLOSE, wxT("Close")) ;
        btn->SetFont(*font) ;
        root->Add(btn, flags) ;

        Bind(wxEVT_BUTTON, [this](auto&) {
            Show(false) ;
        }, wxID_CLOSE) ;

        SetSizerAndFit(root) ;
    }

    AboutDialog::~AboutDialog() noexcept = default ;
}
