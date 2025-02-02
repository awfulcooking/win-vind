#include "lgrparser.hpp"

#include "cmdparser.hpp"
#include "keycode.hpp"
#include "keycodedef.hpp"
#include "keylog.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <stack>
#include <utility>

#include "bind/bindedfunc.hpp"
#include "bind/bindinglist.hpp"
#include "util/container.hpp"
#include "util/def.hpp"


namespace
{
    using namespace vind::core ;

    inline bool is_containing_num(const KeyLog& log) {
        for(const auto& key : log) {
            if(key.is_major_system()) {
                continue ;
            }

            auto uni = keycode_to_unicode(key, log) ;
            if(uni.empty()) {
                continue ;
            }

            auto ascii = uni.front() ;
            if('0' <= ascii && ascii <= '9') {
                return true ;
            }
        }
        return false ;
    }
}


namespace vind
{
    namespace core
    {
        struct LoggerParser::Impl {
            using ParserStateRawType = std::uint32_t ;
            enum ParserState : ParserStateRawType {
                //State
                WAITING             = 0b0000'0001'0000'0000,
                WAITING_IN_NUM      = 0b0000'1001'0000'0000,
                REJECT              = 0b0000'0010'0000'0000,
                REJECT_WITH_SUBSET  = 0b0000'1010'0000'0000,
                ACCEPT_IN_NUM       = 0b0000'0100'0000'0000,
                ACCEPT              = 0b0000'1100'0000'0000,
                ACCEPT_IN_ANY       = 0b0001'0100'0000'0000,

                //Masks
                STATE_MASK      = 0x7f00,
                WAITING_MASK    = 0x0100,
                REJECT_MASK     = 0x0200,
                ACCEPT_MASK     = 0x0400,
                KEYSET_NUM_MASK = 0x00ff,

                //Option flags
                INCLEMENT_CMDIDX= 0x8000,
            } ;


            using LogStatusRawType = std::uint32_t ;
            enum LogStatus : LogStatusRawType {
                //Status
                ALL_FALSE        = 0b0000'0000'0000'0000,
                HAS_KEYSUBSET    = 0b0000'0001'0000'0000,
                ACCEPTED         = 0b0000'0010'0000'0000,
                HAS_KEYSET       = 0b0000'0100'0000'0000,
                ACCEPTED_OPTNUM  = 0b0000'1000'0000'0000,
                WAITING_OPTNUM   = 0b0001'0000'0000'0000,
                ACCEPTED_ANY     = 0b0010'0000'0000'0000,

                //Masks
                STATUS_MASK      = 0xff00,
                MATCHED_NUM_MASK = 0x00ff,
            } ;

            using StateHistory = std::stack<ParserStateRawType, std::vector<ParserStateRawType>> ;

            std::shared_ptr<bind::BindedFunc> func_ ;
            std::shared_ptr<CommandList> cmdlist_ptr_ ;
            StateHistory state_hist_ ;
            std::size_t cmdidx_ ;

            explicit Impl()
            : func_(nullptr),
              cmdlist_ptr_(nullptr),
              state_hist_(),
              cmdidx_(0)
            {}

            explicit Impl(const std::shared_ptr<bind::BindedFunc>& func)
            : func_(func),
              cmdlist_ptr_(nullptr),
              state_hist_(),
              cmdidx_(0)
            {}

            explicit Impl(std::shared_ptr<bind::BindedFunc>&& func)
            : func_(std::move(func)),
              cmdlist_ptr_(nullptr),
              state_hist_(),
              cmdidx_(0)
            {}

            virtual ~Impl() noexcept = default ;

            Impl(Impl&&)                 = default ;
            Impl& operator=(Impl&&)      = default ;
            Impl(const Impl&)            = default ;
            Impl& operator=(const Impl&) = default ;

            LogStatusRawType parse_log(const KeyLog& log) {
                if(!cmdlist_ptr_) {
                    return 0 ;
                }

                LogStatusRawType logstatus = LogStatus::ALL_FALSE ;

                std::size_t most_matched_num = 0 ;

                for(const auto& cmd : *cmdlist_ptr_) {
                    try {
                        const auto& keyset = cmd.at(cmdidx_) ;

                        decltype(most_matched_num) matched_num = 0 ;

                        auto itr = keyset.cbegin() ;
                        if(*itr == KEYCODE_OPTIONAL) {
                            logstatus = LogStatus::ACCEPTED_ANY ;
                            logstatus |= 1 ;
                            return logstatus ;
                        }

                        auto is_last_keyset = (cmdidx_ == cmd.size() - 1) ;

                        if(*itr == KEYCODE_OPTNUMBER) {
                            if(!is_containing_num(log)) {
                                continue ; //not matched
                            }
                            if(keyset.size() == 1) { // just <num>
                                logstatus |= is_last_keyset ?
                                    LogStatus::ACCEPTED_OPTNUM : LogStatus::WAITING_OPTNUM ;
                            }
                        }
                        else if(!log.is_containing(*itr)) {
                            continue ; //not matched
                        }
                        matched_num ++ ;
                        itr ++ ;

                        if(itr != keyset.cend()) { //keyset.size() > 1
                            while(itr != keyset.cend()) {
                                if(*itr == KEYCODE_OPTIONAL) {
                                    logstatus = LogStatus::ACCEPTED_ANY ;
                                    logstatus |= 1 ;
                                    return logstatus ;
                                }

                                if(*itr == KEYCODE_OPTNUMBER && is_containing_num(log)) {
                                    // If keyset.size() is not one (e.g. <num-h>), regards <num> as one digit number.
                                    matched_num ++ ;
                                }
                                else if(log.is_containing(*itr)) {
                                    matched_num ++ ;
                                }
                                itr ++ ;
                            }
                        }

                        if(matched_num == keyset.size()) {
                            logstatus |= LogStatus::HAS_KEYSET ;

                            if(cmdidx_ == cmd.size() - 1) {
                                logstatus |= LogStatus::ACCEPTED ;
                            }
                        }
                        else {
                            logstatus |= LogStatus::HAS_KEYSUBSET ;
                        }

                        if(most_matched_num < matched_num) {
                            most_matched_num = matched_num ;
                        }
                    }
                    catch(const std::out_of_range&) {
                        continue ;
                    }
                }

                // Realistically, we could not type a lot of keys at the same time
                // (e.g. <C-a-b-c-d-e-f-g-h-i-j-k>, abcdjfajfaldABfa),
                // so the actual maximum number is lower-bit of logstatus in this implementation.
                if(most_matched_num < 255) {
                    logstatus |= most_matched_num ;
                }
                else {
                    logstatus |= LogStatus::MATCHED_NUM_MASK ;
                }

                return logstatus ;
            }


            // It parses a log status and return matched num.
            unsigned char parse_log_status(LogStatusRawType status) {
                auto num = static_cast<unsigned char>(status & LogStatus::MATCHED_NUM_MASK) ;

                if(num == 0) {
                    state_hist_.push(ParserState::REJECT) ;
                    return 0 ;
                }

                if(!state_hist_.empty()) {
                    num += state_hist_.top() & ParserState::KEYSET_NUM_MASK ;
                }

                if(status & LogStatus::ACCEPTED_OPTNUM) {
                    state_hist_.push(num | ParserState::ACCEPT_IN_NUM) ;
                }
                else if(status & LogStatus::ACCEPTED_ANY) {
                    state_hist_.push(num | ParserState::ACCEPT_IN_ANY) ;
                }
                else if(status & LogStatus::ACCEPTED) {
                    state_hist_.push(num | ParserState::ACCEPT) ;
                }
                else if(status & LogStatus::WAITING_OPTNUM) {
                    state_hist_.push(num | ParserState::WAITING_IN_NUM) ;
                }
                else if(status & LogStatus::HAS_KEYSET) {
                    state_hist_.push(num | ParserState::WAITING) ;
                }
                else if(status & LogStatus::HAS_KEYSUBSET) {
                    state_hist_.push(num | ParserState::REJECT_WITH_SUBSET) ;
                }
                else {
                    state_hist_.push(ParserState::REJECT) ;
                }
                return num ;
            }

            unsigned char do_waiting(const KeyLog& log) {
                auto status = parse_log(log) ;
                auto res = parse_log_status(status) ;

                // forward command index
                state_hist_.top() |= ParserState::INCLEMENT_CMDIDX ;
                cmdidx_ ++ ;
                return res ;
            }

            unsigned char do_reject_with_keysubset(const KeyLog& UNUSED(log)) {
                state_hist_.push(ParserState::REJECT) ;
                return 0 ;
            }

            unsigned char do_reject(const KeyLog& UNUSED(log)) {
                state_hist_.push(ParserState::REJECT) ;
                return 0 ;
            }

            unsigned char do_accept(const KeyLog& log) {
                state_hist_.push((state_hist_.top() & ParserState::KEYSET_NUM_MASK) | ParserState::WAITING) ;
                return do_waiting(log) ; // Epsilon Transition
            }

            unsigned char do_accept_in_any(const KeyLog& UNUSED(log)) {
                state_hist_.push((state_hist_.top() & ParserState::STATE_MASK) + 1) ; //count <any>'s keycode
                return state_hist_.top() & KEYSET_NUM_MASK ;
            }

            unsigned char do_accept_in_num(const KeyLog& log) {
                if(is_containing_num(log)) {
                    state_hist_.push((state_hist_.top() & ParserState::STATE_MASK) + 1) ; //count <num>'s keycode
                    return state_hist_.top() & KEYSET_NUM_MASK ;
                }

                state_hist_.push(ParserState::WAITING) ;
                return do_waiting(log) ; // Epsilon Transition
            }

            unsigned char do_waiting_in_num(const KeyLog& log) {
                if(is_containing_num(log)) {
                    state_hist_.push((state_hist_.top() & ParserState::STATE_MASK) + 1) ; //count <num>'s keycode
                    return state_hist_.top() & KEYSET_NUM_MASK ;
                }
                return do_waiting(log) ; // Epsilon Transition
            }
        } ;


        LoggerParser::LoggerParser()
        : pimpl(std::make_unique<Impl>())
        {}

        LoggerParser::LoggerParser(
                const std::shared_ptr<bind::BindedFunc>& func)
        : pimpl(std::make_unique<Impl>(func))
        {}
        LoggerParser::LoggerParser(
                std::shared_ptr<bind::BindedFunc>&& func)
        : pimpl(std::make_unique<Impl>(std::move(func)))
        {}

        LoggerParser::~LoggerParser() noexcept = default ;

        LoggerParser::LoggerParser(LoggerParser&&)            = default ;
        LoggerParser& LoggerParser::operator=(LoggerParser&&) = default ;

        LoggerParser::LoggerParser(const LoggerParser& rhs)
        : pimpl(rhs.pimpl ? \
                std::make_unique<Impl>(*(rhs.pimpl)) : \
                std::make_unique<Impl>())
        {}
        LoggerParser& LoggerParser::operator=(const LoggerParser& rhs) {
            if(rhs.pimpl) *pimpl = *(rhs.pimpl) ;
            return *this ;
        }

        void LoggerParser::append_binding(const Command& command) {
            if(command.empty()) return ;
            if(!pimpl->cmdlist_ptr_) {
                pimpl->cmdlist_ptr_ = std::make_shared<CommandList>() ;
            }

            if(pimpl->cmdlist_ptr_.use_count() == 1) {
                pimpl->cmdlist_ptr_->push_back(command) ;
            }
        }

        void LoggerParser::append_binding(const std::string& command) {
            if(command.empty()) return ;
            if(!pimpl->cmdlist_ptr_) {
                pimpl->cmdlist_ptr_ = std::make_shared<CommandList>() ;
            }

            if(pimpl->cmdlist_ptr_.use_count() == 1) {
                pimpl->cmdlist_ptr_->push_back(parse_command(command)) ;
            }
        }

        void LoggerParser::append_binding_list(
                const std::vector<std::string>& list) {
            if(list.empty()) return ;
            if(!pimpl->cmdlist_ptr_) {
                pimpl->cmdlist_ptr_ = std::make_shared<CommandList>() ;
            }

            if(pimpl->cmdlist_ptr_.use_count() == 1) {
                for(const auto& cmd : list) append_binding(cmd) ;
            }
        }
        void LoggerParser::append_binding_list(
                std::vector<std::string>&& list) {
            if(list.empty()) return ;
            if(!pimpl->cmdlist_ptr_) {
                pimpl->cmdlist_ptr_ = std::make_shared<CommandList>() ;
            }
            if(pimpl->cmdlist_ptr_.use_count() == 1) {
                for(auto&& cmd : list) append_binding(std::move(cmd)) ;
            }
        }

        void LoggerParser::reset_binding(const std::string& command) {
            pimpl->cmdlist_ptr_.reset() ;
            append_binding(command) ;
        }
        void LoggerParser::reset_binding(std::string&& command) {
            pimpl->cmdlist_ptr_.reset() ;
            append_binding(std::move(command)) ;
        }

        void LoggerParser::share_parsed_binding_list(
                const std::shared_ptr<CommandList>& cmdlist) {
            pimpl->cmdlist_ptr_.reset() ;
            pimpl->cmdlist_ptr_ = cmdlist ;
        }

        void LoggerParser::reset_binding_list(
                const std::vector<std::string>& list) {
            pimpl->cmdlist_ptr_.reset() ;
            append_binding_list(list) ;
        }
        void LoggerParser::reset_binding_list(
                std::vector<std::string>&& list) {
            pimpl->cmdlist_ptr_.reset() ;
            append_binding_list(std::move(list)) ;
        }

        void LoggerParser::reset_binding_list() {
            pimpl->cmdlist_ptr_.reset() ;
        }

        void LoggerParser::unbind_function() noexcept {
            pimpl->func_ = nullptr ;
        }
        void LoggerParser::bind_function(
                const std::shared_ptr<bind::BindedFunc>& func) {
            if(func) pimpl->func_ = func ;
        }
        void LoggerParser::bind_function(
                std::shared_ptr<bind::BindedFunc>&& func) {
            if(func) pimpl->func_ = std::move(func) ;
        }

        bool LoggerParser::has_function() const noexcept {
            return static_cast<bool>(pimpl->func_) ;
        }

        bool LoggerParser::has_bindings() const noexcept {
            if(!pimpl->cmdlist_ptr_) return false ;
            return !pimpl->cmdlist_ptr_->empty() ;
        }

        const std::shared_ptr<bind::BindedFunc>&
        LoggerParser::get_func() const noexcept {
            return pimpl->func_ ;
        }

        unsigned char LoggerParser::validate_if_match(const KeyLog& log) {
            if(pimpl->state_hist_.empty()) {
                return pimpl->do_waiting(log) ;
            }

            switch(pimpl->state_hist_.top()
                    & Impl::ParserState::STATE_MASK) {

                case Impl::ParserState::WAITING:
                    return pimpl->do_waiting(log) ;

                case Impl::ParserState::WAITING_IN_NUM:
                    return pimpl->do_waiting_in_num(log) ;

                case Impl::ParserState::REJECT:
                    return pimpl->do_reject(log) ;

                case Impl::ParserState::REJECT_WITH_SUBSET:
                    return pimpl->do_reject_with_keysubset(log) ;

                case Impl::ParserState::ACCEPT_IN_NUM:
                    return pimpl->do_accept_in_num(log) ;

                case Impl::ParserState::ACCEPT:
                    return pimpl->do_accept(log) ;

                case Impl::ParserState::ACCEPT_IN_ANY:
                    return pimpl->do_accept_in_any(log) ;

                default:
                    throw LOGIC_EXCEPT("The ParserState is invalid.") ;
            }
        }

        void LoggerParser::reset_state() noexcept {
            Impl::StateHistory().swap(pimpl->state_hist_) ;
            pimpl->cmdidx_ = 0 ;
        }

        void LoggerParser::backward_state(std::size_t n) {
            if(pimpl->state_hist_.size() <= n) {
                reset_state() ;
                return ;
            }

            decltype(n) removed = 0 ;
            while(!pimpl->state_hist_.empty()) {

                if(pimpl->state_hist_.top() \
                        & Impl::ParserState::INCLEMENT_CMDIDX) {
                    if(pimpl->cmdidx_ == 0) {
                        throw LOGIC_EXCEPT("cmdidx is decremented over.") ;
                    }
                    pimpl->cmdidx_ -- ;
                }
                pimpl->state_hist_.pop() ;

                removed ++ ;

                if(removed == n) break ;
            }
        }

        bool LoggerParser::is_accepted() const noexcept {
            if(pimpl->state_hist_.empty()) {
                return false ;
            }
            return pimpl->state_hist_.top() \
                        & Impl::ParserState::ACCEPT_MASK ;
        }

        bool LoggerParser::is_rejected() const noexcept {
            if(pimpl->state_hist_.empty()) {
                return false ;
            }
            return (pimpl->state_hist_.top() \
                        & Impl::ParserState::STATE_MASK) \
                            == Impl::ParserState::REJECT ;
        }
        bool LoggerParser::is_rejected_with_ready() const noexcept {
            if(pimpl->state_hist_.empty()) {
                return false ;
            }
            return (pimpl->state_hist_.top() \
                        & Impl::ParserState::STATE_MASK) \
                            == Impl::ParserState::REJECT_WITH_SUBSET ;
        }

        bool LoggerParser::is_waiting() const noexcept {
            if(pimpl->state_hist_.empty()) {
                return true ;
            }
            return pimpl->state_hist_.top() \
                        & Impl::ParserState::WAITING_MASK ;
        }

        std::size_t LoggerParser::state_stack_size() const noexcept {
            return pimpl->state_hist_.size() ;
        }
    }
}
