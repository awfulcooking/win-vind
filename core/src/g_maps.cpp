#include "g_maps.hpp"

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "disable_gcc_warning.hpp"

#include <nlohmann/json.hpp>

#include "enable_gcc_warning.hpp"

#include "bind/bindings_lists.hpp"
#include "err_logger.hpp"
#include "mapdefs.hpp"
#include "mode.hpp"
#include "parser/bindings_parser.hpp"
#include "path.hpp"
#include "util/def.hpp"


namespace
{
    using namespace vind ;

    const auto& func_list = bindingslists::get() ;

    template <typename T>
    bool is_func_name(T&& name) {
        auto id = BindedFunc::name_to_id(std::forward<T>(name)) ;
        for(const auto& f : func_list) {
            if(id == f->id()) return true ;
        }
        return false ;
    }


    using MapModeList = ModeArray<std::unordered_map<std::size_t, gmaps::UniqueMap>> ;
    MapModeList g_mode_maps{} ;

    using namespace nlohmann ;
    json g_default_maps{} ;
}


namespace vind
{
    namespace gmaps {
        struct UniqueMap::Impl {
            Command in_ ;
            std::string out_ ;
            bool is_function_ ;

            template <typename T1, typename T2>
            explicit Impl(T1&& in, T2&& out, bool is_function)
            : in_(bindparser::parse_string_binding(std::forward<T1>(in))),
              out_(std::forward<T2>(out)),
              is_function_(is_function)
            {
                for(auto& keyset : in_) {
                    if(keyset.empty()) {
                        throw LOGIC_EXCEPT("Invalid Trigger Command") ;
                    }
                }
            }

            template <typename T1, typename T2>
            explicit Impl(T1&& in, T2&& out)
            : Impl(in, out, is_func_name(out))
            {}

            explicit Impl()
            : in_(),
              out_(),
              is_function_(false)
            {}
        } ;

        UniqueMap::UniqueMap()
        : pimpl(std::make_shared<Impl>())
        {}

        UniqueMap::UniqueMap(
                const std::string& in,
                const std::string& out)
        : pimpl(std::make_shared<Impl>(in, out))
        {}

        UniqueMap::UniqueMap(
                const std::string& in,
                const std::string& out,
                bool is_function)
        : pimpl(std::make_shared<Impl>(in, out, is_function))
        {}

        bool UniqueMap::is_function() const noexcept {
            return pimpl->is_function_ ;
        }

        const Command& UniqueMap::trigger_command() const noexcept {
            return pimpl->in_ ;
        }

        const std::string& UniqueMap::func_name() const noexcept {
            return pimpl->out_ ;
        }

        std::size_t UniqueMap::func_id() const noexcept {
            return BindedFunc::name_to_id(pimpl->out_) ;
        }

        Command UniqueMap::create_target_command() const {
            return bindparser::parse_string_binding(pimpl->out_) ;
        }

        std::size_t UniqueMap::compute_hash() const {
            std::string strcmd{} ;
            for(auto& set : pimpl->in_) {
                for(auto& key : set) {
                    strcmd.push_back(key) ;
                }
            }
            return std::hash<std::string>()(std::move(strcmd)) ;
        }


        void initialize() {
            g_default_maps.clear() ;
            std::ifstream ifs(path::to_u8path(path::Default::BINDINGS())) ;
            ifs >> g_default_maps ;
        }

        void reset() {
            MapModeList().swap(g_mode_maps) ;

            for(const auto& obj : g_default_maps) {
                std::size_t modeidx ;

                auto pretty_error = [&modeidx, &obj] (auto& e) {
                    std::stringstream ss ;
                    ss << e.what() << " (" << mode::get_mode_strcode(modeidx) \
                        << " in" << obj.dump() << ")" ;
                    return ss.str() ;
                } ;

                try {
                    if(!obj.is_object()) {
                        PRINT_ERROR("Invalid Syntax " + obj.dump()) ;
                        continue ;
                    }

                    const auto& name = obj.at("name").get<std::string>() ;
                    if(name.empty()) {
                        throw LOGIC_EXCEPT("Invalid Function Name") ;
                    }

                    for(modeidx = 0 ; modeidx < mode::mode_num() ; modeidx ++) {

                        auto modekey = mode::get_mode_strcode(modeidx) ;
                        if(modekey.empty()) {
                            continue ;
                        }

                        auto cmdlist = obj.at(modekey) ;
                        if(cmdlist.empty() || !cmdlist.is_array()) {
                            continue ;
                        }

                        //If there are some key-bindings fields of the mode having <mode-name> (e.q. <guin>, <edin>) in bindings.json ,
                        //they are copied key-bindings from the first mode in json-array to them.
                        //Ex) "guin": ["<Esc>", "happy"]
                        //    "edin": ["<guin>", "<guii>"]    -> same as "guin"'s key-bindings(<Esc>, "happy")
                        if(cmdlist.size() == 1) {
                            auto copy_mode = bindparser::parse_string_modecode(cmdlist.front()) ;
                            if(copy_mode != mode::Mode::None) {
                                cmdlist = obj.at(mode::get_mode_strcode(copy_mode)) ;
                            }
                        }

                        auto& maps = g_mode_maps[modeidx] ;
                        for(std::string strcmd : cmdlist) {
                            if(strcmd.empty()) {
                                continue ;
                            }
                            UniqueMap map(strcmd, name, true) ;
                            maps[map.compute_hash()] = std::move(map) ;
                        }
                    }
                }
                catch(const std::out_of_range& e) {
                    PRINT_ERROR(pretty_error(e)) ;
                    continue ;
                }
                catch(const json::out_of_range& e) {
                    PRINT_ERROR(pretty_error(e)) ;
                    continue ;
                }
                catch(const std::logic_error& e) {
                    throw std::logic_error(pretty_error(e)) ;
                }
                catch(const std::runtime_error& e) {
                    throw std::runtime_error(pretty_error(e)) ;
                }
            }
        }

        void map(
                const std::string& incmd,
                const std::string& outcmd,
                mode::Mode mode) {
            if(incmd.empty() || outcmd.empty()) {
                throw RUNTIME_EXCEPT("Empty maps") ;
            }

            UniqueMap map(incmd, outcmd) ;

            if(map.is_function()) {
                // Overwrite if function-name or empty
                auto hash = map.compute_hash() ;

                auto& maps = g_mode_maps[static_cast<int>(mode)] ;
                try {
                    auto& oldmap = maps.at(hash) ;
                    if(oldmap.is_function()) {
                        oldmap = std::move(map) ;
                    }
                }
                catch(const std::out_of_range&) {
                    maps[hash] = std::move(map) ;
                }
            }
            else {
                // Overwrite map anyway
                g_mode_maps[static_cast<int>(mode)][map.compute_hash()] = std::move(map) ;
            }
        }

        void unmap(
                const std::string& incmd,
                mode::Mode mode) {
            if(incmd.empty()) {
                throw RUNTIME_EXCEPT("Empty map") ;
            }
            UniqueMap map(incmd, "", true) ;
            auto hash = map.compute_hash() ;

            g_mode_maps[static_cast<int>(mode)].erase(hash) ;
        }

        void mapclear(mode::Mode mode) {
            g_mode_maps[static_cast<int>(mode)].clear() ;
        }

        UniqueMap get_map(
                const std::string& cmd,
                mode::Mode mode) {
            UniqueMap map(cmd, "", true) ;
            return g_mode_maps[static_cast<int>(mode)].at(map.compute_hash()) ;
        }

        void get_maps(
                mode::Mode mode,
                std::vector<UniqueMap>& returns) {
            if(!returns.empty()) {
                returns.clear() ;
            }
            for(auto& [hash, map] : g_mode_maps[static_cast<int>(mode)]) {
                returns.push_back(map) ;
            }
        }
    }
}
