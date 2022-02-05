/* 
 * Mod Loader Utilities Headers
 * Created by LINK/2012 <dma_2012@hotmail.com>
 * 
 *  This file provides helpful functions for plugins creators.
 * 
 *  This source code is offered for use in the public domain. You may
 *  use, modify or distribute it freely.
 *
 *  This code is distributed in the hope that it will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAIMED. This includes but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 */
#ifndef MODLOADER_UTIL_DETOUR_HPP
#define	MODLOADER_UTIL_DETOUR_HPP
#pragma once
#include <string>
#include <modloader/modloader.hpp>
#include <modloader/util/injector.hpp>
#include <modloader/util/path.hpp>
#include <tuple>

namespace modloader
{
    // Tag type
    struct tag_detour_t {};
    static const tag_detour_t tag_detour;
    
    struct tag_many_detour_t {};
    static const tag_many_detour_t tag_many_detour;


    /*
     *  File detour
     *      Hooks a call to detour a string argument call to open another file instead of the sent to the function
     *      @addr is the address to hook 
     *      Note the object is dummy (has no content), all stored information is static
     */
    template<class Traits, class MyHooker, class Ret, class ...Args>
    struct basic_file_detour : public MyHooker
    {
        protected:
            typedef MyHooker                     function_hooker;
            typedef typename MyHooker::func_type func_type;


            /*
             *  Used to set up a printer on each function call that prints what is getting loaded.
             *  Notice incref() should be called before you set up your hook over the function.
             *  Static and reference counted hook.
             */
            class printer_t : public MyHooker
            {
                public:
                    // Before calling the printer, those variables should be set up by the user
                    bool print_next = false;        // Prints on the next print call
                    bool is_custom  = false;        // Is loading a custom file
                    bool use_lpath  = false;        // Prints lpath for the file path instead of the argument
                    const char* lpath;              // When use_lpath=true, this gets used as the file path to print

                private:
                    MyHooker hook;
                    uint32_t ref = 0;

                    Ret print(func_type fun, Args&... args)
                    {
                        if(this->print_next)
                        {
                            static const char* what = Traits::what();
                            static const auto  pos  = Traits::arg - 1;
                            auto& arg = std::get<pos>(std::forward_as_tuple(args...));

                            this->print_next = false;
                            if(this->has_info_for_print())
                                plugin_ptr->Log("Loading %s %s \"%s\"",
                                                (is_custom? "custom" : "default"), what, (use_lpath? lpath : arg));
                        }
                        return fun(args...);
                    }

                public:
                    // Constructors, move constructors, assigment operators........
                    // nope and nope, just default construction
                    printer_t() = default;
                    printer_t(const printer_t&) = delete;
                    printer_t(printer_t&& rhs) = delete;
                    printer_t& operator=(const printer_t& rhs) = delete;
                    printer_t& operator=(printer_t&& rhs) = delete;

                    ~printer_t()
                    {
                        this->restore();
                    }

                    // Makes the hook of the printer
                    // If the hook has already been installed by another function hooker of this same trait
                    // just increase a reference counter...
                    void incref()
                    {
                        if(this->ref != 0)
                            ++this->ref;
                        else
                        {
                            this->ref = 1;
                            MyHooker::make_call([this](func_type func, Args&... args) -> Ret
                            {
                                return this->print(std::move(func), args...);
                            });
                        }
                    }

                    // Restores the call we've replaced for the printer
                    // If the reference counter isn't unique, just decreases it
                    void decref()
                    {
                        if(this->ref)   // make sure there's any reference
                        {
                            if(this->ref != 1)
                                --this->ref;
                            else
                            {
                                this->ref = 0;
                                MyHooker::restore();
                            }
                        }
                    }

                    // Checks if we have enought information about the trait and the plugin to print anything
                    static bool has_info_for_print()
                    {
                        static const char* what = Traits::what();
                        return (what && what[0] && plugin_ptr);
                    }

                    // The instance of a printer
                    // Uses a shared_ptr to avoid static destruction order problems
                    static std::shared_ptr<printer_t> instance()
                    {
                        static auto p = std::shared_ptr<printer_t>(has_info_for_print()? new printer_t() : nullptr);
                        return p;
                    }
            };

            // Store for the path (relative to gamedir)
            std::string path, temp;
            std::function<std::string(std::string)> transform;
            std::function<std::string(std::string)> postransform;
            std::shared_ptr<printer_t> printer;

            // The detoured function goes to here
            Ret hook(func_type fun, Args&... args)
            {
                static const auto  pos  = Traits::arg - 1;
                static const char* what = Traits::what();

                auto& arg = std::get<pos>(std::forward_as_tuple(args...));
                char fullpath[MAX_PATH];
                const char* lpath = nullptr;
                
                auto& path = this->temp;
                path.assign(this->path);

                // If has transform functor, use it to get new path
                if(this->transform)
                    path = this->transform(arg);

                if(!path.empty())   // Has any path to override the original with?
                {
                    if(this->postransform)
                        path = this->postransform(std::move(path));

                    if(!path.empty())
                    {
                        if(!IsAbsolutePath(path))
                            copy_cstr(fullpath, fullpath + MAX_PATH, plugin_ptr->loader->gamepath, path.c_str());
                        else
                            strcpy(fullpath, path.c_str());

                        // Make sure the file exists, if it does, change the parameter
                        if(GetFileAttributesA(fullpath) != INVALID_FILE_ATTRIBUTES)
                        {
                            arg = fullpath;
                            lpath = path.c_str();
                        }
                    }
                }

                // Set ups the printer to print the next loading file
                if(printer && !printer->print_next) // avoid double setuping
                {
                    printer->print_next = true;
                    printer->lpath      = lpath;
                    printer->is_custom  = (lpath != 0);
                    printer->use_lpath  = (lpath != 0);
                }

                // Call the function with the new filepath
                return fun(args...);
            }
        
        public:

            // Constructors, move constructors, assigment operators........
            basic_file_detour() : printer(printer_t::instance()) {}
            basic_file_detour(const basic_file_detour&) = delete;
            basic_file_detour(basic_file_detour&& rhs)
                : MyHooker(std::move(rhs)), path(std::move(rhs.path)),
                transform(std::move(rhs.transform)), postransform(rhs.postransform),
                printer(rhs.printer)    // (dont move the printer)
            {}
            basic_file_detour& operator=(const basic_file_detour& rhs) = delete;
            basic_file_detour& operator=(basic_file_detour&& rhs)
            {
                MyHooker::operator=(std::move(rhs));
                this->path = std::move(rhs.path);
                this->transform = std::move(rhs.transform);
                this->postransform = std::move(rhs.postransform);
                this->printer = rhs.printer; // (dont move the printer)
                return *this;
            }

            explicit basic_file_detour(std::function<std::string(std::string)> on_transform)
                : basic_file_detour()
            {
                this->OnTransform(std::move(on_transform));
            }

            ~basic_file_detour()
            {
                this->restore();
            }

            // Makes the hook
            void make_call()
            {
                if(!this->has_hooked())
                {
                    if(printer) printer->incref();
                    MyHooker::make_call([this](func_type func, Args&... args) -> Ret
                    {
                        return this->hook(std::move(func), args...);
                    });
                }
            }

            // Restore need to also restore the printer
            void restore()
            {
                if(printer && this->has_hooked()) printer->decref();
                MyHooker::restore();
            }

            // Sets the file to override with
            void setfile(std::string path)
            {
                this->make_call();
                this->path = std::move(path);
            }

            
            //
            // the following callbacks setupers follows file_overrider naming conventions not function_hookers
            //

            void OnTransform(std::function<std::string(std::string)> functor)
            {
                this->transform = std::move(functor);
            }

            void OnPosTransform(std::function<std::string(std::string)> functor)
            {
                this->postransform = std::move(functor);
            }
    };


    /*
        file overrider
            Made to easily override some game file
            Works better in conjunction with a file detour (basic_file_detour)
    */
    struct file_overrider
    {
        protected:
            using injection_list_t = std::vector<std::unique_ptr<scoped_base>>;

            injection_list_t injections;                    // List of injections attached to this overrider
            const modloader::file* file = nullptr;          // The file being used as overrider
            bool bCanReinstall = false;                     // Can this overrider get reinstalled?
            bool bCanUninstall = false;                     // Can this overrider get uninstalled?

            // Reinitialization time
            bool bReinitAfterStart = false;                 // Should call Reinit after game startup? (The game screen is there)
            bool bReinitAfterLoad = false;                  // Should call Reinit after game load? (After loading screen)

            // Events
            std::function<void(const modloader::file*)> mOnChange;      // Called when the file changes
            std::function<void(const modloader::file*)> mInstallHook;   // Install (or reinstall, or uninstall if file is null) hook for file
            std::function<void()>                       mReload;        // Reload the file on the game

        public:
            file_overrider() = default;                 
            file_overrider(const file_overrider&) = delete;     // cba to implement those
            file_overrider(file_overrider&&) = delete;          // ^^

            // Injections accessors
            size_t NumInjections() { return injections.size(); }
            scoped_base& GetInjection(size_t i) { return *injections.at(i); }
            
            // Checks if at this point of the game execution we can install stuff
            bool CanInstall()
            {
                return (!plugin_ptr->loader->has_game_started || bCanReinstall);
            }

            // Checks if at this point of the game execution we can uninstall stuff
            bool CanUninstall()
            {
                return (!plugin_ptr->loader->has_game_started || bCanUninstall);
            }

            // Call to install/reinstall/uninstall the file (no file should be installed)
            bool InstallFile(const modloader::file& file)
            {
                if(this->CanInstall())
                    return PerformInstall(&file);
                return false;
            }

            //
            bool Refresh()
            {
                return ReinstallFile();
            }

            // Reinstall the currently installed file
            bool ReinstallFile()
            {
                // Can reinstall if the game hasn't started or if we can reinstall this kind
                if(this->CanInstall())
                    return PerformInstall(this->file);
                return false;
            }

            // Uninstall the currently installed file
            bool UninstallFile()
            {
                // Can uninstall if the game hasn't started or if we can uninstall this kind
                if(this->CanUninstall())
                    return PerformInstall(nullptr);
                return false;
            }

            // Set functor to call when it's necessary to install a hook for a file
            void OnHook(std::function<void(const modloader::file*)> fun)
            {
                this->mInstallHook = std::move(fun);
            }

            // Set functor to call when it's necessary to reload the file
            void OnReload(std::function<void()> fun)
            {
                this->mReload = std::move(fun);
            }

            // Set functor to call whenevr the file changes
            void OnChange(std::function<void(const modloader::file*)> fun)
            {
                this->mOnChange = std::move(fun);
            }

            // Set the file detourer (important to call, otherwise no effect will happen)
            // This overrides OnHook
            template<class ...Args> void SetFileDetourTuple(const std::tuple<Args...>& ddtuple)
            {
                using tuple_type = std::tuple<Args...>;
                static const size_t tuple_size = std::tuple_size<tuple_type>::value;

                this->injections.resize(tuple_size);
                HlpSetFileDetour<tuple_type>(std::integral_constant<size_t, 0>(), ddtuple);

                OnHook([this](const modloader::file* f)
                {
                    Hlp_SetFile<tuple_type>(std::integral_constant<size_t, 0>(), f);
                });
            }

            template<class ...Args> void SetFileDetour(Args&&... args)
            {
                return SetFileDetourTuple(std::forward_as_tuple(std::forward<Args>(args)...));
            }

            // Optionally set up the calls for all detours even if no file has been received to override
            template<class... Detours>
            void MakeCallForAllDetours()
            {
                using tuple_type = std::tuple<Detours...>;
                Hlp_MakeCall<tuple_type>(std::integral_constant<size_t, 0>());
            }

        public: // Helpers for construction
            // Pack of basic boolean states to help the initialization of this object
            struct params
            {
                bool bCanReinstall, bCanUninstall, bReinitAfterStart, bReinitAfterLoad;
                
                params(bool bCanReinstall, bool bCanUninstall, bool bReinitAfterStart, bool bReinitAfterLoad) :
                    bCanReinstall(bCanReinstall), bCanUninstall(bCanUninstall), bReinitAfterStart(bReinitAfterStart), bReinitAfterLoad(bReinitAfterLoad)
                {}

                params(std::nullptr_t) : params(false, false, false, false)
                {}
            };

            file_overrider& SetParams(const params& p)
            {
                this->bCanReinstall     = p.bCanReinstall;
                this->bCanUninstall     = p.bCanUninstall;
                this->bReinitAfterStart = p.bReinitAfterStart;
                this->bReinitAfterLoad  = p.bReinitAfterLoad;
                return *this;
            }


            // Constructor - only params, more liberty
            file_overrider(const params& p) :
                bCanReinstall(p.bCanReinstall), bCanUninstall(p.bCanUninstall), bReinitAfterStart(p.bReinitAfterStart), bReinitAfterLoad(p.bReinitAfterLoad)
            {}

            // Constructor - Quickly setup the entire file overrider based on a detourer
            template<class DetourType>
            file_overrider(tag_detour_t, const params& p, DetourType detour, std::function<void()> reload) :
                file_overrider(tag_detour, p, std::move(detour))
            {
                this->OnReload(std::move(reload));
            }

            // Constructor - Quickly setup the entire file overrider based on many detourers
            template<class... Detours>
            file_overrider(tag_detour_t, const params& p, const std::tuple<Detours...>& detours,  std::function<void()> reload) :
                file_overrider(tag_detour, p, detours)
            {
                this->OnReload(std::move(reload));
            }

            // Constructor - Quickly setup the entire file overrider based on a detourer
            template<class DetourType>
            file_overrider(tag_detour_t, const params& p, DetourType detour) :
                file_overrider(p)
            {
                this->SetFileDetour(std::move(detour));
            }

            // Constructor - Quickly setup the entire file overrider based on many detourers
            template<class... Detours>
            file_overrider(tag_detour_t, const params& p, const std::tuple<Detours...>& detours) :
                file_overrider(p)
            {
                this->SetFileDetourTuple(detours);
            }

            // Installs the necessary hooking to load the specified file
            // (done automatically but you can call it manually if you want to always have the hook in place)
            void InstallHook()
            {
                if(mInstallHook) mInstallHook(this->file);
            }

        protected:
            // Perform a install for the specified file, if null, it will uninstall the currently installed file
            bool PerformInstall(const modloader::file* file)
            {
                this->file = file;
                if(mOnChange) mOnChange(this->file);
                InstallHook(); TryReload();
                return true;
            }

            // Tries to reload (if necessary) the current file
            void TryReload()
            {
                // Go ahead only if we have a reload functor and the game has booted up
                if(mReload && plugin_ptr->loader->has_game_started)
                {
                    if(plugin_ptr->loader->has_game_loaded)
                    {
                        // Can reload after game load?
                        if(bReinitAfterLoad) mReload();
                    }
                    else
                    {
                        // Can reload after game startup?
                        if(bReinitAfterStart) mReload();
                    }
                }
            }

        protected:

            template<class Tuple>
            void Hlp_SetFile(std::integral_constant<size_t, std::tuple_size<Tuple>::value>, const modloader::file* f)
            {}

            template<class Tuple, size_t I>
            void Hlp_SetFile(std::integral_constant<size_t, I>, const modloader::file* f)
            {
                using T = std::decay_t<typename std::tuple_element<I, Tuple>::type>;
                T& detourer = static_cast<T&>(GetInjection(I));
                detourer.setfile(f? f->filepath() : "");
                return Hlp_SetFile<Tuple>(std::integral_constant<size_t, I+1>(), f); 
            }

            template<class Tuple>
            void Hlp_MakeCall(std::integral_constant<size_t, std::tuple_size<Tuple>::value>)
            {}

            template<class Tuple, size_t I>
            void Hlp_MakeCall(std::integral_constant<size_t, I>)
            {
                using T = std::decay_t<typename std::tuple_element<I, Tuple>::type>;
                T& detourer = static_cast<T&>(GetInjection(I));
                detourer.make_call();
                return Hlp_MakeCall<Tuple>(std::integral_constant<size_t, I+1>()); 
            }

            template<class Tuple>
            void HlpSetFileDetour(std::integral_constant<size_t, std::tuple_size<Tuple>::value>, const Tuple& tuple)
            {}

            template<class Tuple, size_t I>
            void HlpSetFileDetour(std::integral_constant<size_t, I>, const Tuple& tuple)
            {
                using Arg = typename std::tuple_element<I, Tuple>::type;
                injections.at(I).reset(new std::decay_t<Arg>(std::forward<Arg>(std::get<I>(tuple))));
                return HlpSetFileDetour<Tuple>(std::integral_constant<size_t, I+1>(), tuple);
            }

    };

}


#endif	/* MODLOADER_UTIL_HPP */

