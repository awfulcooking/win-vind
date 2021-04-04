#include "bind/mouse/jump_keybrd.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "coreio/i_params.hpp"
#include "io/keybrd.hpp"
#include "io/screen_metrics.hpp"
#include "key/key_absorber.hpp"
#include "key/key_log.hpp"
#include "key/keycodecvt.hpp"
#include "coreio/err_logger.hpp"
#include "bind/mouse/jump_keybrd.hpp"
#include "coreio/path.hpp"
#include "util/def.hpp"
#include "util/string.hpp"
#include "entry.hpp"


namespace vind
{

    struct Jump2Any::Impl {
        float max_keybrd_xposs = 0 ;
        float max_keybrd_yposs = 0 ;

        using key_pos_t = std::array<float, 256> ;
        key_pos_t xposs{} ;
        key_pos_t yposs{} ;
    } ;

    Jump2Any::Jump2Any()
    :pimpl(std::make_unique<Impl>())
    {}
    Jump2Any::~Jump2Any() noexcept            = default ;
    Jump2Any::Jump2Any(Jump2Any&&)            = default ;
    Jump2Any& Jump2Any::operator=(Jump2Any&&) = default ;

    const std::string Jump2Any::sname() noexcept {
        return "jump_to_any" ;
    }

    void Jump2Any::sprocess(
            bool first_call,
            unsigned int UNUSED(repeat_num),
            KeycodeLogger* const UNUSED(parent_keycodelgr),
            const CharLogger* const UNUSED(parent_charlgr)) const {
        if(!first_call) return ;

        //reset key state (binded key)
        
        keyabsorber::InstantKeyAbsorber ika ;

        //ignore toggle keys (for example, CapsLock, NumLock, IME....)
        const auto toggle_keys = keyabsorber::get_pressed_list() ;

        RECT rect ;
        screenmetrics::get_conbined_metrics(&rect) ;

        const auto width  = screenmetrics::width(rect) ;
        const auto height = screenmetrics::height(rect) ;

        while(vind::update_background()) {
            if(keyabsorber::is_pressed(KEYCODE_ESC)) return ;

            const auto log = keyabsorber::get_pressed_list() - toggle_keys ;
            if(log.empty()) continue ;

            try {
                for(const auto& keycode : log) {
                    if(keycodecvt::is_unreal_key(keycode))
                        continue ;

                    auto x_pos = static_cast<int>( \
                            pimpl->xposs[keycode] / pimpl->max_keybrd_xposs * width) ;
                    auto y_pos = static_cast<int>( \
                            pimpl->yposs[keycode] / pimpl->max_keybrd_yposs * height) ;

                    if(x_pos == width) 
                        x_pos -= iparams::get_i("screen_pos_buf") ;

                    if(y_pos == height) 
                        y_pos -= iparams::get_i("screen_pos_buf") ;

                    SetCursorPos(x_pos, y_pos) ;

                    for(const auto& key : log) {
                        keybrd::release_keystate(key) ;
                    }
                    return ;
                }
            }
            catch(const std::out_of_range&) {
                continue ;
            }
        }
    }

    void Jump2Any::load_config() {
        //initilize
        pimpl->max_keybrd_xposs = 0 ;
        pimpl->max_keybrd_yposs = 0 ;

        pimpl->xposs.fill(0) ;
        pimpl->yposs.fill(0) ;
        const auto filename = path::KEYBRD_MAP() ;

        std::ifstream ifs(path::to_u8path(filename), std::ios::in) ;
        std::string buf ;
        int lnum = 0 ;

        auto ep = [&lnum, &buf, &filename](auto msg) {
            PRINT_ERROR(buf + msg + "\"" + filename + "\", L" + std::to_string(lnum) + ".") ;
        } ;

        while(getline(ifs, buf)) {
            try {
                lnum ++ ;

                if(buf.empty()) {
                    continue ;
                }

                //if top character is #, this line is regarded as comment-out.
                if(buf.front() == '#') {
                    continue ;
                }

                const auto vec = util::split(buf, " ") ;

                if(vec.size() != 3) {
                    ep(" is bad syntax in ") ;
                    continue ;
                }

                const auto x = std::stof(vec[0]) ;
                const auto y = stof(vec[1]) ;

                if(x > pimpl->max_keybrd_xposs) pimpl->max_keybrd_xposs = x ;
                if(y > pimpl->max_keybrd_yposs) pimpl->max_keybrd_yposs = y ;

                //specific code
                auto code = vec[2] ;
                //is ascii code
                if(code.size() == 1) {
                    if(const auto keycode = keycodecvt::get_keycode(code.front())) {
                        //overwrite
                        pimpl->xposs[keycode] = x ;
                        pimpl->yposs[keycode] = y ;
                        continue ;
                    }
                    ep(" is not supported in ") ;
                    continue ;
                }

                code = util::A2a(code) ;
                if(code.front() != '<' && code.back() != '>') {
                    ep(" is bad syntax in ") ;
                }

                code = code.substr(1, code.length() - 2) ;
                if(code == "space") {
                    auto&& keycode = keycodecvt::get_keycode(' ') ;
                    pimpl->xposs[keycode] = x ;
                    pimpl->yposs[keycode] = y ;
                    continue ;
                }

                if(auto keycode = keycodecvt::get_sys_keycode(code)) {
                    pimpl->xposs[keycode] = x ;
                    pimpl->yposs[keycode] = y ;
                    continue ;
                }

                ep(" is invalid system key code in ") ;
            }
            catch(const std::runtime_error& e) {
                PRINT_ERROR(e.what()) ;
                continue ;
            }
        }
    }

}