#include "bind/hotkey/clipboard.hpp"

#include <windows.h>
#include <iostream>

#include "bind/base/mode.hpp"

#include "util/def.hpp"

#include "bind/emu/edi_change_mode.hpp"
#include "bind/mode/change_mode.hpp"
#include "io/keybrd.hpp"
#include "io/mouse.hpp"
#include "key/key_absorber.hpp"

namespace vind
{
    //CBCopy
    const std::string CBCopy::sname() noexcept {
        return "cb_copy" ;
    }

    void CBCopy::sprocess(
            bool first_call,
            unsigned int UNUSED(repeat_num),
            KeycodeLogger* const UNUSED(parent_keycodelgr),
            const CharLogger* const UNUSED(parent_charlgr)) {
        if(!first_call) return ;
        mouse::release(KEYCODE_MOUSE_LEFT) ;

        //there are cases in which not editable.
        //thus, not use Message Copy
        keybrd::pushup(KEYCODE_LCTRL, KEYCODE_C) ;
    }


    //CBPaste
    const std::string CBPaste::sname() noexcept {
        return "cb_paste" ;
    }

    void CBPaste::sprocess(
            bool first_call,
            unsigned int UNUSED(repeat_num),
            KeycodeLogger* const UNUSED(parent_keycodelgr),
            const CharLogger* const UNUSED(parent_charlgr)) {
        if(!first_call) return ;

        mouse::release(KEYCODE_MOUSE_LEFT) ;

        //not selecting at paste.
        keybrd::pushup(KEYCODE_LCTRL, KEYCODE_V) ;
    }


    //CBCut
    const std::string CBCut::sname() noexcept {
        return "cb_cut" ;
    }

    void CBCut::sprocess(
            bool first_call,
            unsigned int UNUSED(repeat_num),
            KeycodeLogger* const UNUSED(parent_keycodelgr),
            const CharLogger* const UNUSED(parent_charlgr)) {
        if(!first_call) return ;

        mouse::release(KEYCODE_MOUSE_LEFT) ;
        keybrd::pushup(KEYCODE_LCTRL, KEYCODE_X) ;
    }


    //CBDelete
    const std::string CBDelete::sname() noexcept {
        return "cb_delete" ;
    }

    void CBDelete::sprocess(
            bool first_call,
            unsigned int UNUSED(repeat_num),
            KeycodeLogger* const UNUSED(parent_keycodelgr),
            const CharLogger* const UNUSED(parent_charlgr)) {
        if(!first_call) return ;
        mouse::release(KEYCODE_MOUSE_LEFT) ;

        //selecting->cut
        //unselecting->delete
        keybrd::pushup(KEYCODE_LCTRL, KEYCODE_C) ;
        keybrd::pushup(KEYCODE_DELETE) ;
    }


    //CBBackSpace
    const std::string CBBackSpace::sname() noexcept {
        return "cb_back_space" ;
    }

    void CBBackSpace::sprocess(
            bool first_call,
            unsigned int UNUSED(repeat_num),
            KeycodeLogger* const UNUSED(parent_keycodelgr),
            const CharLogger* const UNUSED(parent_charlgr)) {
        if(!first_call) return ;
        mouse::release(KEYCODE_MOUSE_LEFT) ;

        //selecting->cut
        //unselecting->delete
        keybrd::pushup(KEYCODE_LCTRL, KEYCODE_C) ;
        keybrd::pushup(KEYCODE_BKSPACE) ;
    }
}