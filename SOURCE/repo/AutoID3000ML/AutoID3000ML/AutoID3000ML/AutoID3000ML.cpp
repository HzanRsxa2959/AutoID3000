/*
 *  Mod Loader Plugin Base File
 *  Use this (copy-pasting) to help you in starting a plugin project
 * 
 */
#include "include/modloader-master/include/modloader/modloader.hpp"

/*
 *  The plugin object
 */
class ThePlugin : public modloader::basic_plugin
{
    public:
        const info& GetInfo();

        bool OnStartup();
        bool OnShutdown();
        int GetBehaviour(modloader::file&);
        bool InstallFile(const modloader::file&);
        bool ReinstallFile(const modloader::file&);
        bool UninstallFile(const modloader::file&);
        void Update();
        
} thePlugin;

REGISTER_ML_PLUGIN(::thePlugin);
//>
#include "plugin.h"
#include <map>
#include <set>
#include <filesystem>
#include "include/modloader-master/include/modloader/util/path.hpp"
#include "CKeyGen.h"

using namespace plugin;
using namespace std;
namespace fs = filesystem;

auto initializationstatus = int(0);
#define INITIALIZATIONDONE if (initializationstatus == 0) {

auto logfile = fstream();
auto logpath = (fs::path(PLUGIN_PATH("")) / fs::path(thePlugin.GetInfo().name)).replace_extension(".log");
class LogWrite {
public:
	LogWrite() {
		logfile.open(logpath, fstream::out);
		logfile << (string("This is the log file for the plugin \"") + string(thePlugin.GetInfo().name) + string("\", and will be only updated further after the game has loaded.\n")).c_str();
		logfile << "If that is not the case, then the plugin may have failed to start; check \"./modloader/modloader.log\" for errors.";
		logfile.close();
	}
} logWrite;

auto combineStrings(vector<string> stringslist) {
	auto combinedstring = string();
	for (auto liststring : stringslist) combinedstring += liststring;
	auto combinedchar = new char[combinedstring.length() + 1];
	strcpy(combinedchar, combinedstring.c_str());
	return combinedchar;
}

#define FILEPATH const char *
#define FILEEXTENSION FILEPATH
auto filescached = map<FILEEXTENSION, vector<FILEPATH> *>();
auto filesall = set<pair<FILEPATH, FILEEXTENSION>>();
#define FILEPAIR make_pair(file.filepath(), combineStrings({ ".", file.filext() }))

auto caseLower(string casedstring) {
	auto lowercasedstring = string();
	for (auto stringcharacter : casedstring) {
		if (
			stringcharacter >= 'A'
			&& stringcharacter <= 'Z'
			) {
			stringcharacter = tolower(stringcharacter);
		}
		lowercasedstring += stringcharacter;
	}
	return lowercasedstring;
}

#define EXTENSIONSTRING "_ml_"
const auto extensionlength = sizeof(EXTENSIONSTRING) - 1;

#define FOLDERSTRING "_modloader_"

auto hashkey = unsigned int(0);
//<

/*
 *  ThePlugin::GetInfo
 *      Returns information about this plugin 
 */
const ThePlugin::info& ThePlugin::GetInfo()
{
    //static const char* extable[] = { "dff", "txd", "fxp", 0 };
    //static const info xinfo      = { "Plugin Name", "Version", "Author", -1, extable };
//>
	static const char* extable[] = { 0 };
	static const info xinfo = { "AutoID3000ML", "1000", "HzanRsxa2959", 1, extable };
//<
    return xinfo;
}





/*
 *  ThePlugin::OnStartup
 *      Startups the plugin
 */
bool ThePlugin::OnStartup()
{
//>
	Events::initGameEvent.before += [] {
		logfile.open(logpath, fstream::out);

		auto autoid3000version = int(0);
		auto autoid3000module = GetModuleHandle("#AutoID3000.asi");
		if (autoid3000module) {
			typedef int (__cdecl *modversionpointer)();
			auto modversion = (modversionpointer)GetProcAddress(autoid3000module, "modVersion");
			if (modversion) {
				autoid3000version = modversion();
			}
		}
		if (autoid3000version < 3002) {
			initializationstatus = 2;

			logfile << "AutoID3000 3002 or later required.";
			logfile.close();

			return;
		}

		logfile << "File(s): " << filesall.size() << "\n";
		for (auto mlfile : filesall) logfile << "\t" << mlfile.second << " : " << mlfile.first << '\n';
		logfile.close();

		initializationstatus = 1;
	};
//<
    return true;
}

/*
 *  ThePlugin::OnShutdown
 *      Shutdowns the plugin
 */
bool ThePlugin::OnShutdown()
{
//>
//<
    return true;
}


/*
 *  ThePlugin::GetBehaviour
 *      Gets the relationship between this plugin and the file
 */
int ThePlugin::GetBehaviour(modloader::file& file)
{
//>
	INITIALIZATIONDONE
		if (!file.is_dir()) {
			auto fileextension = string(file.filext());
			auto extensionlength = fileextension.length();
			if (
				(
						extensionlength > extensionlength
					&&	caseLower(fileextension.substr(0, extensionlength)) == EXTENSIONSTRING
				)
				||	modloader::IsFileInsideFolder(caseLower(file.filepath()), false, FOLDERSTRING)
				) {
				file.behaviour = file.hash | (hashkey == 0 ? CKeyGen::GetKey(thePlugin.GetInfo().name) : hashkey);
				return MODLOADER_BEHAVIOUR_YES;
			}
			if (extensionlength > 0) return MODLOADER_BEHAVIOUR_CALLME;
		}
	}
//<
    return MODLOADER_BEHAVIOUR_NO;
}

/*
 *  ThePlugin::InstallFile
 *      Installs a file using this plugin
 */
bool ThePlugin::InstallFile(const modloader::file& file)
{
//>
	INITIALIZATIONDONE
		filesall.insert(FILEPAIR);
		return true;
	}
//<
    return false;
}

/*
 *  ThePlugin::ReinstallFile
 *      Reinstall a file previosly installed that has been updated
 */
bool ThePlugin::ReinstallFile(const modloader::file& file)
{
//>
//<
    return false;
}

/*
 *  ThePlugin::UninstallFile
 *      Uninstall a previosly installed file
 */
bool ThePlugin::UninstallFile(const modloader::file& file)
{
//>
	INITIALIZATIONDONE
		filesall.erase(FILEPAIR);
		return true;
	}
//<
    return false;
}

/*
 *  ThePlugin::Update
 *      Updates the state of this plugin after a serie of install/uninstalls
 */
void ThePlugin::Update()
{
//>
//<
}

//>
#define EXPORTFUNCTION EXTERN_C __MIDL_DECLSPEC_DLLEXPORT auto

EXPORTFUNCTION getFiles(FILEEXTENSION filesextension, int *filesamount) {
	if (initializationstatus != 2) {
		auto &filefind = filescached.find(filesextension);
		if (filefind == filescached.end()) {
			auto filesvector = new vector<FILEPATH>();

			auto extensionslower = caseLower(filesextension);
			for (auto mlfile : filesall) {
				if (
					extensionslower == ""
					|| caseLower(mlfile.second) == extensionslower
					) {
					filesvector->push_back(mlfile.first);
				}
			}

			filescached[filesextension] = filesvector;

			*filesamount = filesvector->size();
			return filesvector->data();
		}
		else {
			auto &filesvector = filefind->second;

			*filesamount = filesvector->size();
			return filesvector->data();
		}
	}
	else {
		*filesamount = NULL;
		return (const char **)nullptr;
	}
}
EXPORTFUNCTION getStem(FILEPATH filepath, char *filepart) {
	strcpy(filepart, fs::path(filepath).stem().string().c_str());
}
EXPORTFUNCTION getParent(FILEPATH filepath, char *filepart) {
	strcpy(filepart, fs::path(filepath).parent_path().string().c_str());
}
//<