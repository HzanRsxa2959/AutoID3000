#include "plugin.h"
//>
#include <filesystem>
#include <regex>
#include <atlbase.h>
#include <chrono>
#include <map>
#include <set>
#include "include/IniReader-master/IniReader.h"
#include "CStreaming.h"
#include "include/CRCpp-release-1.1.0.0/inc/CRC.h"
#include "CGenericGameStorage.h"
#include "CGangs.h"
#include "CPickups.h"
#include "CCarGenerator.h"
#include "CKeyGen.h"
#include "CClothes.h"
#include "CRadar.h"
#include "CFileLoader.h"
//<

using namespace plugin;
//>
using namespace std;

namespace fs = filesystem;

#define EXPORTFUNCTION extern "C" __declspec(dllexport)

auto modversion = int(3003);
EXPORTFUNCTION auto modVersion() {
	return modversion;
}
auto modname = string("AutoID3000");
auto modMessage(string messagetext, int messageflags = MB_OK) {
	return MessageBox(GetActiveWindow(), messagetext.c_str(), modname.c_str(), messageflags);
}

auto locationdata = fs::path("data");

auto folderroot = fs::path(GAME_PATH(""));
auto folderdata = folderroot / locationdata;
auto folderscripts = folderroot / fs::path("scripts");
auto folderml = folderroot / fs::path("modloader");
auto foldermod = folderroot / fs::path(modname);
auto folderCreate(fs::path foldername) {
	auto folderpath = foldermod / foldername;
	fs::create_directories(folderpath);
	return folderpath;
}
auto folderlogs = folderCreate("logs");
auto folderbackups = folderCreate("backups");
auto folderstorage = folderCreate("storage");

class LogFile {
public:
	fstream filehandle;
	LogFile(fs::path filestem) {
		filehandle.open(folderlogs / filestem.replace_extension(".log"), fstream::out);
	}
	auto newLine() {
		filehandle << endl;
	}
	auto writeText(string logtext) {
		filehandle << logtext;
		newLine();
	}
};
auto logmain = LogFile("main");
auto logbackups = LogFile("backups");
auto logfiles = LogFile("files");
auto logassigned = LogFile("assigned");
auto logignored = LogFile("ignored");
auto logfree = LogFile("free");
auto logunassigned = LogFile("unassigned");

class LoadedModule {
public:
	fs::path modulepath;
	bool isinstalled = false;
	LoadedModule(fs::path modulestem, fs::path mlfolder = "", bool noextension = true) {
		auto modulename = modulestem;
		if(noextension) modulename.replace_extension(".asi");
		if (fs::exists(folderroot / modulename)) {
			modulepath = folderroot;
			isinstalled = true;
		}
		else if (fs::exists(folderscripts / modulename)) {
			modulepath = folderscripts;
			isinstalled = true;
		}
		else if (
			mlfolder.string().length() > 0
			&& fs::exists(folderml / mlfolder / modulename)
			) {
			modulepath = folderml / mlfolder;
			isinstalled = true;
		}
	}
};

#define MODULEFLA string("fastman92 Limit Adjuster")
#define MODULEOLA string("Open Limit Adjuster")

auto loadedml = LoadedModule("modloader");
auto loadedfla = LoadedModule("$fastman92limitAdjuster", MODULEFLA);
auto loadedola = LoadedModule("III.VC.SA.LimitAdjuster.", MODULEOLA);

auto lowerString(string casedstring) {
	auto loweredstring = string();
	for (auto stringcharacter : casedstring) {
		if (
			stringcharacter >= 'A'
			|| stringcharacter <= 'Z'
			) {
			stringcharacter = tolower(stringcharacter);
		}
		loweredstring += stringcharacter;
	}
	return loweredstring;
}
auto lowermod = lowerString((foldermod / fs::path()).string());
auto lowerml = lowerString((folderml / fs::path()).string());

auto getCasing(std::wstring const &srcPath, std::wstring &dstPath) {
	HRESULT hr = 0;
	CComPtr<IDispatch> disp;
	CoInitialize(NULL);
	hr = disp.CoCreateInstance(L"Scripting.FileSystemObject");
	if (FAILED(hr)) return hr;
	CComVariant src(srcPath.c_str()), dst;
	hr = disp.Invoke1(L"GetAbsolutePathName", &src, &dst);
	if (FAILED(hr)) return hr;
	SIZE_T cch = SysStringLen(dst.bstrVal);
	dstPath = std::wstring(dst.bstrVal, cch);
	return hr;
}
auto casePath(fs::path path) {
	auto pathreturn = path;
	auto &pathcurrent = path.wstring();
	auto pathcased = wstring();
	if (!FAILED(getCasing(pathcurrent, pathcased))) {
		pathreturn = pathcased;
	}
	return pathreturn;
}

auto hashFile(fs::path filelocation) {
	auto filepath = fs::path(filelocation);
	auto filehash = int(0);
	auto filehandle = fstream();
	filehandle.open(filepath, fstream::in | fstream::binary);
	if (filehandle.is_open()) {
		auto filesize = fs::file_size(filepath);
		auto filebuffer = vector<char>(filesize);
		if (filehandle.read(filebuffer.data(), filesize)) {
			filehash = CRC::Calculate(filebuffer.data(), filebuffer.size(), CRC::CRC_32());
		}
		filehandle.close();
	}
	return filehash;
}

auto copyWait(fs::path sourcepath, fs::path targetpath, bool showmessage = false) {
	if (hashFile(sourcepath) == hashFile(targetpath)) {
		if (showmessage) modMessage("Both files are identical.");
		return;
	}

	targetpath = casePath(targetpath);
	fs::create_directories(targetpath.parent_path());
	auto temporarypath = targetpath;
	temporarypath.replace_extension(temporarypath.extension().string() + "." + modname + "temp");
	fs::remove(temporarypath);
	fs::copy_file(sourcepath, temporarypath);
	auto temporaryfile = fstream();
	while (true) {
		temporaryfile.open(temporarypath);
		if (temporaryfile.is_open()) {
			break;
		}
		Sleep(1);
	}
	temporaryfile.close();
	fs::remove(targetpath);
	fs::rename(temporarypath, targetpath);
	fs::remove(temporarypath);
}

auto outputroot = (casePath(folderroot) / fs::path()).string();
auto OutputPath(fs::path filepath, string matchpath = outputroot, bool nodot = false) {
	filepath = casePath(filepath);
	auto pathstring = filepath.string();
	while (true) {
		auto pathfind = pathstring.find(matchpath);
		if (pathfind == string::npos) break;
		pathstring.erase(pathfind, matchpath.length());
	}
	auto retpath = fs::path(pathstring);
	if (!nodot) retpath = fs::path(".") / retpath;
	return retpath.string();
}

auto storagebackups = fs::path();

class BackupFile {
public:
	fs::path originalpath;
	fs::path backuppath;
	bool originalexisted = false;
	auto backupExists() {
		if (fs::exists(backuppath)) {
			return true;
		}
		return false;
	}
	auto deleteBackup() {
		if (backupExists()) {
			fs::remove(backuppath);
			logbackups.writeText("Deleted backup of file \"" + OutputPath(originalpath) + "\" at \"" + OutputPath(backuppath) + "\".");
		}
	}
	auto restoreBackup() {
		if (
			originalexisted
			|| backupExists()
			) {
			copyWait(backuppath, originalpath, true);
			logbackups.writeText("Restored backup of file \"" + OutputPath(originalpath) + "\" at \"" + OutputPath(backuppath) + "\".");
			deleteBackup();
		}
	}
	BackupFile(fs::path originallocation) {
		originalpath = casePath(originallocation);
		backuppath = folderbackups / originalpath.filename();
		if (fs::exists(originalpath)) {
			originalexisted = true;
		}
	}
	auto initializeBackup() {
		if (backupExists()) {
			if (modMessage("File \"" + OutputPath(originalpath) + "\" already found backed-up at \"" + OutputPath(backuppath) + "\". Restore it before deleting?", MB_YESNO | MB_ICONWARNING) == IDYES) {
				originalexisted = true;
				restoreBackup();
			}
			deleteBackup();
		}
	}
	auto createBackup() {
		if (originalexisted) {
			copyWait(originalpath, storagebackups / backuppath.filename());

			copyWait(originalpath, backuppath);
			logbackups.writeText("Created backup of file \"" + OutputPath(originalpath) + "\" at \"" + OutputPath(backuppath) + "\".");
		}
	}
	auto warnBackup() {
		if (originalexisted) return true;

		if (modMessage("File \"" + OutputPath(originalpath) + "\" not found when it was needed. Would you like for the game to automatically close after loading? The file will probably have been generated by then.", MB_YESNO | MB_ICONERROR) == IDYES) {
			Events::processScriptsEvent += [] {
				modMessage("The game will now automatically close.", MB_ICONWARNING);
				exit(EXIT_SUCCESS);
			};
		}
		return false;
	}
};
//limiter>
auto backupflaconfig = BackupFile(loadedfla.modulepath / fs::path("fastman92limitAdjuster_GTASA").replace_extension(".ini"));
auto backupflaweapon = BackupFile(loadedfla.modulepath / locationdata / fs::path("gtasa_weapon_config").replace_extension(".dat"));
auto backupflaaudio = BackupFile(loadedfla.modulepath / locationdata / fs::path("gtasa_vehicleAudioSettings").replace_extension(".cfg"));
auto backupflamodel = BackupFile(loadedfla.modulepath / locationdata / fs::path("model_special_features").replace_extension(".dat"));
auto backupflablip = BackupFile(loadedfla.modulepath / locationdata / fs::path("gtasa_radarBlipSpriteFilenames").replace_extension(".dat"));
auto backupflacombo = BackupFile(loadedfla.modulepath / locationdata / fs::path("gtasa_melee_config").replace_extension(".dat"));
auto backupflatrain = BackupFile(loadedfla.modulepath / locationdata / fs::path("gtasa_trainTypeCarriages").replace_extension(".dat"));
//<limiter

#define CHARIGNORE '$'
#define CHARPED '_'
#define CHARVEHICLE '^'
#define CHARBIKE 'b'
#define CHARFLYING 'f'
#define CHARWATER 'w'
//limiter>
#define CHARWEAPON '&'
#define CHARCLOTH '@'
#define CHARBLIP ','
#define CHARCOMBO '%'
#define CHARTRAIN ')'
//<limiter
#define CHARDAMAGE '`'
#define CHARTIMED '+'
#define CHARHIER '-'
#define CHARINFO '.'
#define CHARCOL CHARWEAPON
#define CHAROLA CHARCLOTH
#define CHARML CHARINFO

#define SECTIONEND "end:"
#define SECTIONDEF "def:"
#define SECTIONIDE "ide:"
#define SECTIONIPL "ipl:"
#define SECTIONTXT "txt:"
#define SECTIONFIL "fil: "
#define SECTIONFLA "fla:"
#define SECTIONFLB "flb:"
#define SECTIONFLC "flc:"
//limiter>
#define SECTIONWPN "wpn:"
#define SECTIONBLP "blp:"
#define SECTIONBLT "blt:"
#define SECTIONMEL "mel:"
#define SECTIONTRN "trn:"
//<limiter
#define SECTIONFLS "fls:"
#define SECTIONSPC "spc:"

#define EXTENSIONAUID3 ".auid3"
#define EXTENSIONIDE ".ide"
#define EXTENSIONIPL ".ipl"
#define EXTENSIONTXT ".txt"
#define EXTENSIONDAT ".dat"

#define LOGINDENT string("\t") +

auto filesAll(fs::path searchpath, void(*searchcallback)(fs::path, fs::recursive_directory_iterator)) {
	if (fs::exists(searchpath)) {
		auto searchhandle = fs::recursive_directory_iterator(searchpath);
		auto searchend = fs::recursive_directory_iterator();
		while (searchhandle != searchend) {
			searchcallback(searchhandle->path(), searchhandle);
			++searchhandle;
		}
	}
}
auto filesml = set<fs::path>();
auto filesauid3 = filesml;
auto fileside = filesml;
auto filestxt = filesml;

auto Gfilesection = bool(false);
auto Gfileskip = bool(false);
auto Glinenumber = int(0);

auto Gauid3current = fs::path();
auto Gauid3path = fs::path();
auto Gauid3fla = bool(false);

auto auid3names = set<string>();

auto linesFile(fs::path filepath, bool logfile, void(*filecallback)(string, string)) {
	auto filehandle = fstream();
	filehandle.open(filepath);
	if (filehandle.is_open()) {
		auto fileline = string();
		while (getline(filehandle, fileline)) {
			auto linelower = lowerString(fileline);
			filecallback(fileline, linelower);
		}
		if (logfile) {
			logfiles.writeText(LOGINDENT OutputPath(filepath));
		}
		filehandle.close();
	}
}
auto linesfiles = map<fs::path, vector<string>>();
auto  linesola = set<string>{
		"dynamic limits:ptrnode singles"
	,	"dynamic limits:ptrnode doubles"
	,	"dynamic limits:entryinfonodes"
	,	"dynamic limits:peds"
	,	"dynamic limits:pedintelligence"
	,	"dynamic limits:vehicles"
	,	"ipl:buildings"
	,	"dynamic limits:objects"
	,	"ipl:dummies"
	,	"dynamic limits:colmodels"
	,	"dynamic limits:tasks"
	,	"dynamic limits:events"
	,	"dynamic limits:pointroute"
	,	"dynamic limits:patrolroute"
	,	"dynamic limits:noderoute"
	,	"dynamic limits:taskallocator"
	,	"dynamic limits:pedattractors"
	,	"dynamic limits:vehiclestructs"
	,	"dynamic limits:matrices"
	,	"water limits:blocks to be rendered outside world"
	,	"visibility limits:alpha entity list limit"
	,	"renderer limits:visible entity pointers"
	,	"renderer limits:visible lod pointers"
	,	"dynamic limits:rwobjectinstances"
	,	"ide limits:ide objects type 1"
	,	"ide limits:ide objects type 2"
	,	"ide limits:timed objects"
	,	"ide limits:hier objects"
	,	"ide limits:vehicle models"
	,	"ide limits:ped models"
	,	"ide limits:weapon models"
	,	"ipl:inst entries per file"
	,	"ipl:entity index array"
	,	"shadow limits:static shadows"
	,	"other limits:coronas"
	,	"streaming:memory available"
};
auto linesAdd(fs::path linessection, string sectionline) {
	if (
		linessection == SECTIONFLA
		|| linessection == SECTIONFLB
		|| linessection == SECTIONFLC
		) {
		if (sectionline[0] != CHAROLA) {
			auto olaline = sectionline;
			auto olaseparator = olaline.find_last_of(':');
			if (olaseparator != string::npos) {
				olaline = olaline.substr(0, olaseparator);
				olaseparator = olaline.find_last_of(':');
				if (olaseparator != string::npos) olaline = olaline.substr(0, olaseparator);
			}
			if (linesola.find(lowerString(olaline)) != linesola.end()) sectionline = CHAROLA + sectionline;
		}
		if (
			sectionline.length() > 0
			&& sectionline[0] == CHAROLA
			) {
			if (loadedola.isinstalled) return;
			sectionline = sectionline.substr(1);
		}
	}
	auto linesfound = linesfiles.find(linessection);
	if (linesfound == linesfiles.end()) {
		auto filelines = vector<string>();
		filelines.push_back(sectionline);
		linesfiles[linessection] = filelines;
	}
	else {
		auto &filelines = linesfound->second;
		filelines.push_back(sectionline);
	}
}
auto lineside = vector<string>();

auto AUID3Iterator(string auid3section, bool auid3skip, void(*auid3callback)(string, string)) {
	auto &auid3find = linesfiles.find(auid3section);
	if (auid3find != linesfiles.end()) {
		auto &auid3lines = auid3find->second;
		for (auto auid3line : auid3lines) {
			if (
				!auid3skip
				|| auid3line.length() > 0
				) {
				auid3callback(auid3line, lowerString(auid3line));
			}
		}
		auid3lines.clear();
	}
}

auto FLAScan(string flaline, string *flasection, string *flakey, bool hasvalue, int *fladefault = nullptr, int *flavalue = nullptr) {
	auto flaseparator = flaline.find_first_of(':');
	if (flaseparator != string::npos) {
		*flasection = flaline.substr(0, flaseparator);
		flaline = flaline.substr(flaseparator + 1);

		if (!hasvalue) {
			*flakey = flaline;

			return true;
		}

		flaseparator = flaline.find_first_of(':');
		if (flaseparator != string::npos) {
			*flakey = flaline.substr(0, flaseparator);
			flaline = flaline.substr(flaseparator + 1);

			if (sscanf(flaline.c_str(), "%d:%d", fladefault, flavalue) == 2) {
				return true;
			}
		}
	}
	return false;
}
auto Gflafile = fstream();
auto FLAWrite(fs::path filepath, string flasection, bool skipemptylines) {
	Gflafile.open(filepath, fstream::app);
	if (Gflafile.is_open()) {
		Gflafile << '\n';
		AUID3Iterator(flasection, skipemptylines, [](string flaline, string flalower) {
			Gflafile << flaline << '\n';
		});
		Gflafile.close();
	}
}
auto flaini = new CIniReader;
auto flasection = string();
auto flakey = string();
auto flavalue = int();
auto fladefault = int();
auto FLAApply() {
	AUID3Iterator(SECTIONFLC, true, [](string flaline, string flalower) {
		if (FLAScan(flalower, &flasection, &flakey, true, &fladefault, &flavalue)) {
			delete(flaini); flaini = new CIniReader(backupflaconfig.originalpath.string());
			if (flavalue > flaini->ReadInteger(flasection, flakey, fladefault)) flaini->WriteInteger(flasection, flakey, flavalue);
		}
	});
	AUID3Iterator(SECTIONFLA, true, [](string flaline, string flalower) {
		if (FLAScan(flalower, &flasection, &flakey, true, &fladefault, &flavalue)) {
			delete(flaini); flaini = new CIniReader(backupflaconfig.originalpath.string());
			flaini->WriteInteger(flasection, flakey, flaini->ReadInteger(flasection, flakey, fladefault) + flavalue);
		}
	});
	AUID3Iterator(SECTIONFLB, true, [](string flaline, string flalower) {
		if (FLAScan(flalower, &flasection, &flakey, false)) {
			delete(flaini); flaini = new CIniReader(backupflaconfig.originalpath.string());
			flaini->WriteInteger(flasection, flakey, 1);
		}
	});
	delete(flaini);
}

auto limitkillables = int(800);
auto limitmodels = int(20000);
//limiter>
auto limitweapons = int(70);
auto limitblips = int(64);
auto limitcombos = int(17);
auto limittrains = int(16);
//<limiter

auto modelsused = set<int>();
auto modelScan(string modelline) {
	auto modelindex = modelline.find_first_of(", ");
	if (modelindex != string::npos) {
		auto modelstring = modelline.substr(0, modelindex);
		int modelid;
		if (sscanf(modelstring.c_str(), "%d", &modelid) == 1) {
			modelsused.insert(modelid);
		}
	}
}

//limiter>
#define SCANNAME weaponScan
#define SCANUSED weaponsused
#define SCANFUNCTION auto SCANUSED = modelsused; \
auto SCANNAME(string flaline) { \
	auto flaseparator = flaline.find_first_of(' '); \
	if (flaseparator != string::npos) { \
		auto flastring = flaline.substr(0, flaseparator); \
		auto flaid = int(); \
		if (sscanf(flastring.c_str(), "%d", &flaid) == 1) { \
			SCANUSED.insert(flaid); \
		} \
	} \
}
SCANFUNCTION

#undef SCANNAME
#define SCANNAME blipScan
#undef SCANUSED
#define SCANUSED blipsused
SCANFUNCTION

#undef SCANNAME
#define SCANNAME comboScan
#undef SCANUSED
#define SCANUSED combosused
SCANFUNCTION

#undef SCANNAME
#define SCANNAME trainScan
#undef SCANUSED
#define SCANUSED trainsused
SCANFUNCTION
//<limiter

auto freekillables = modelsused;
auto freemodels = modelsused;
//limiter>
auto freeweapons = modelsused;
auto freeblips = modelsused;
auto freecombos = modelsused;
auto freetrains = modelsused;
//<limiter

auto auid3sids = map<fs::path, int>();

class SaveGameEntity {
public:
	int id;
	string name;
};
#define SAVEINHERIT : public SaveGameEntity
//limiter>
class SavePlayerWeapon SAVEINHERIT {
public:
	SavePlayerWeapon() {}
	int ammo; string sammo = "ammo";
	bool current; string scurrent = "current";
	int clip; string sclip = "clip";
	SavePlayerWeapon(
			int weaponid
		,	string weaponname
		,	int weaponammo
		,	bool weaponcurrent
		,	int weaponclip
	) {
		id = weaponid;
		name = weaponname;
		ammo = weaponammo;
		current = weaponcurrent;
		clip = weaponclip;
	}
};
class SaveGangWeapon SAVEINHERIT {
public:
	SaveGangWeapon() {}
	int gang; string sgang = "gang";
	int weapon; string sweapon = "weapon";
	SaveGangWeapon(
			int weaponid
		,	string weaponname
		,	int weapongang
		,	int weaponweapon
	) {
		id = weaponid;
		name = weaponname;
		gang = weapongang;
		weapon = weaponweapon;
	}
};
class SavePickupModel SAVEINHERIT {
public:
	SavePickupModel() {}
	int pickup; string spickup = "pickup";
	SavePickupModel(
			int modelid
		,	string modelname
		,	int modelpickup
	) {
		id = modelid;
		name = modelname;
		pickup = modelpickup;
	}
};
class SaveGarageCar SAVEINHERIT {
public:
	SaveGarageCar() {}
	int garage; string sgarage = "garage";
	SaveGarageCar(
			int carid
		,	string carname
		,	int cargarage
	) {
		id = carid;
		name = carname;
		garage = cargarage;
	}
};
class SaveGeneratorCar SAVEINHERIT {
public:
	SaveGeneratorCar() {}
	int generator; string sgenerator = "generator";
	SaveGeneratorCar(
			int carid
		,	string carname
		,	int cargenerator
	) {
		id = carid;
		name = carname;
		generator = cargenerator;
	}
};
class SaveCarMod SAVEINHERIT {
public:
	SaveCarMod() {}
	int mod; string smod = "mod";
	int garage; string sgarage = "garage";
	SaveCarMod(
			int modid
		,	string modname
		,	int modmod
		,	int modgarage
	) {
		id = modid;
		name = modname;
		mod = modmod;
		garage = modgarage;
	}

};
class SaveClothModel SAVEINHERIT {
public:
	SaveClothModel() {}
	int part; string spart = "part";
	SaveClothModel(
			int id
		,	string modelname
		,	int modelpart
	) {
		id = 0;
		name = modelname;
		part = modelpart;
	}
};
class SaveClothTexture SAVEINHERIT {
public:
	SaveClothTexture() {}
	int part; string spart = "part";
	SaveClothTexture(
			int id
		,	string modelname
		,	int modelpart
	) {
		id = 0;
		name = modelname;
		part = modelpart;
	}
};
class SaveBlipSprite SAVEINHERIT {
public:
	SaveBlipSprite() {}
	int blip; string sblip = "blip";
	SaveBlipSprite(
			int spriteid
		,	string spritename
		,	int spriteblip
	) {
		id = spriteid;
		name = spritename;
		blip = spriteblip;
	}
};
class SavePlayerCombo SAVEINHERIT {
public:
	SavePlayerCombo() {}
	string scombo = "combo";
	SavePlayerCombo(
			int comboid
		,	string comboname
	) {
		id = comboid;
		name = comboname;
	}
};
auto saveplayerweapons = vector<SavePlayerWeapon>();
auto savegangweapons = vector<SaveGangWeapon>();
auto savepickupmodels = vector<SavePickupModel>();
auto savegaragecars = vector<SaveGarageCar>();
auto savegeneratorcars = vector<SaveGeneratorCar>();
auto savecarmods = vector<SaveCarMod>();
auto saveclothmodels = vector<SaveClothModel>();
auto saveclothtextures = vector<SaveClothTexture>();
auto saveblipsprites = vector<SaveBlipSprite>();
auto saveplayercombo = SavePlayerCombo(0, "");
//<limiter
auto saveini = fs::path();
auto saveINI(CIniReader *inifile, fs::path inistem) {
	inifile->~CIniReader();
	new(inifile) CIniReader((saveini / inistem.replace_extension(".ini")).string());
}
//limiter>
auto saveiniplayerweapons = CIniReader();
auto saveinigangweapons = saveiniplayerweapons;
auto saveinipickupmodels = saveiniplayerweapons;
auto saveinigaragecars = saveiniplayerweapons;
auto saveinigeneratorcars = saveiniplayerweapons;
auto saveinicarmods = saveiniplayerweapons;
auto saveiniclothmodels = saveiniplayerweapons;
auto saveiniclothtextures = saveiniplayerweapons;
auto saveiniblipsprites = saveiniplayerweapons;
auto saveiniplayercombo = saveiniplayerweapons;
//<limiter
auto savePaths(fs::path savepath) {
	auto savehash = hashFile(savepath);
	saveini = savepath.parent_path() / fs::path(modname) / fs::path(to_string(savehash));
	//limiter>
	saveINI(&saveiniplayerweapons, "playerweapons");
	saveINI(&saveinigangweapons, "gangweapons");
	saveINI(&saveinipickupmodels, "pickupmodels");
	saveINI(&saveinigaragecars, "garagecars");
	saveINI(&saveinigeneratorcars, "generatorcars");
	saveINI(&saveinicarmods, "carmods");
	saveINI(&saveiniclothmodels, "clothmodels");
	saveINI(&saveiniclothtextures, "clothtextures");
	saveINI(&saveiniblipsprites, "blipsprites");
	saveINI(&saveiniplayercombo, "playercombo");
	//<limiter
}

auto pointersread = bool(false);
auto playerpointer = (CPlayerPed *)nullptr;
//limiter>
auto idsauid3s = map<int, string>();
auto weaponsauid3s = idsauid3s;
auto clothesauid3s = idsauid3s;
auto blipsauid3s = idsauid3s;
auto combosauid3s = idsauid3s;
auto trainsauid3s = idsauid3s;
//<limiter

EXPORTFUNCTION auto AUID3ID(const char *auid3) {
	auto id = int(-1);
	auto find = auid3sids.find(auid3);
	if (find != auid3sids.end()) {
		id = find->second;
	}
	return id;
}

//limiter>
#define APINAME modelAUID3
#define APIMAP idsauid3s
#define APIFUNCTION EXPORTFUNCTION auto APINAME(int id) { \
	auto auid3 = ""; \
	auto find = APIMAP.find(id); \
	if (find != APIMAP.end()) { \
		auid3 = find->second.c_str(); \
	} \
	return auid3; \
}
APIFUNCTION

#undef APINAME
#define APINAME weaponAUID3
#undef APIMAP
#define APIMAP weaponsauid3s
APIFUNCTION

#undef APINAME
#define APINAME blipAUID3
#undef APIMAP
#define APIMAP blipsauid3s
APIFUNCTION

#undef APINAME
#define APINAME comboAUID3
#undef APIMAP
#define APIMAP combosauid3s
APIFUNCTION

#undef APINAME
#define APINAME trainAUID3
#undef APIMAP
#define APIMAP trainsauid3s
APIFUNCTION
//<limiter

#define POINTERSSTART unsigned int
#define POINTERSSIZE unsigned char

//limiter>
auto gangsstart = POINTERSSTART();
auto pickupsstart = POINTERSSTART();
auto garagesstart = POINTERSSTART();
auto generatorsstart = POINTERSSTART();
auto blipsstart = POINTERSSTART();

auto gangssize = POINTERSSIZE();
auto pickupssize = POINTERSSIZE();
auto garagessize = POINTERSSIZE();
auto generatorssize = POINTERSSIZE();
auto modssize = POINTERSSIZE();
auto blipssize = POINTERSSIZE();

auto gangsweapon = POINTERSSIZE();
auto pickupsmodel = POINTERSSIZE();
auto garagesmodel = POINTERSSIZE();
auto generatorsmodel = POINTERSSIZE();
auto modsmodel = POINTERSSIZE();
auto blipssprite = POINTERSSIZE();

auto gangsamount = POINTERSSTART();
auto pickupsamount = POINTERSSTART();
auto garagesamount = POINTERSSIZE();
auto generatorsamount = POINTERSSTART();
auto modsamount = POINTERSSTART();
auto blipsamount = POINTERSSTART();

#define SETGANGSWEAPON SetUInt
#define SETPICKUPSMODEL SetUShort
#define SETGARAGESMODEL SETPICKUPSMODEL
#define SETGENERATORSMODEL SETPICKUPSMODEL
#define SETMODSMODEL SETPICKUPSMODEL
#define SETBLIPSSPRITE SetUChar

#define GETGANGSWEAPON GetUInt
#define GETPICKUPSMODEL GetUShort
#define GETGARAGESMODEL GETPICKUPSMODEL
#define GETGENERATORSMODEL GETPICKUPSMODEL
#define GETMODSMODEL GETPICKUPSMODEL
#define GETBLIPSSPRITE GetUChar

auto gangsPointer(int gangid) {
	return (CGangInfo *)(gangsstart + (gangid * gangssize));
}
auto pickupsPointer(int pickupid) {
	return (CPickup *)(pickupsstart + (pickupid * pickupssize));
}
auto garagesPointer(int garageid) {
	return (garagesstart + (garageid * garagessize));
}
auto generatorsPointer(int generatorid) {
	return (CCarGenerator *)(generatorsstart + (generatorid * generatorssize));
}
auto modsPointer(int modid, int garageid) {
	return ((modid * modssize) + garagesPointer(garageid));
}
auto blipsPointer(int blipid) {
	return (tRadarTrace *)(blipsstart + (blipid * blipssize));
}

auto gangsWeapon(int gangid, int weaponindex) {
	return POINTERSSTART(gangsPointer(gangid)) + gangsweapon + (weaponindex * 0x4);
}
auto pickupsModel(int pickupid) {
	return POINTERSSTART(pickupsPointer(pickupid)) + pickupsmodel;
}
auto garagesModel(int garageid) {
	return POINTERSSTART(garagesPointer(garageid)) + garagesmodel;
}
auto generatorsModel(int generatorid) {
	return POINTERSSTART(generatorsPointer(generatorid)) + generatorsmodel;
}
auto modsModel(int modid, int garageid) {
	return POINTERSSTART(modsPointer(modid, garageid)) + modsmodel;
}
auto blipsSprite(int blipid) {
	return POINTERSSTART(blipsPointer(blipid)) + blipssprite;
}

#define CLOTHESMODELS playerpointer->m_pPlayerData->m_pPedClothesDesc->m_anModelKeys
#define CLOTHESTEXTURES playerpointer->m_pPlayerData->m_pPedClothesDesc->m_anTextureKeys

#define PLAYERCOMBO playerpointer->m_nFightingStyle
//<limiter
auto clothesHash(string clothname) {
	if (clothname.length() > 2) {
		clothname = clothname.substr(2);
	}
	else {
		clothname = "";
	}
	return CKeyGen::GetUppercaseKey(clothname.c_str());
}
auto savePointers() {
	if (!pointersread) {
		playerpointer = FindPlayerPed();

		//limiter>
		gangsstart = patch::GetUInt(0x5D3A93);
		pickupsstart = patch::GetUInt(0x48ADC3);
		garagesstart = patch::GetUInt(0x443A88) + 0x4;
		generatorsstart = patch::GetUInt(0x6F3F88);
		blipsstart = patch::GetUInt(0x5D5868) - 0x20;

		gangssize = patch::GetUChar(0x5D3AA1);
		pickupssize = patch::GetUChar(0x4590E1);
		garagessize = patch::GetUChar(0x5D3309);
		generatorssize = patch::GetUChar(0x6F32AA);
		modssize = patch::GetUChar(0x447E8D);
		blipssize = patch::GetUChar(0x5D58DF);

		gangsweapon = 0x4;
		pickupsmodel = patch::GetUChar(0x5D35B4);
		garagesmodel = patch::GetUChar(0x447E5F);
		generatorsmodel = 0;
		modsmodel = patch::GetUChar(0x447E6D);
		blipssprite = 0x24;

		gangsamount = patch::GetUInt(0x5D3A98);
		pickupsamount = patch::GetUInt(0x457190);
		garagesamount = patch::GetUChar(0x5D3345);
		generatorsamount = patch::GetUInt(0x6F3F7D);
		modsamount = patch::GetUInt(0x447E6F);
		blipsamount = patch::GetUInt(0x5D5870);
		//<limiter

		pointersread = true;
	}
}
auto saveApply() {
	savePointers();

	#define SETENTITIES saveplayerweapons
	#define SAVEAPPLY \
		for(auto saveentity : SETENTITIES) {
	#define SAVEAPPLY2 \
		} \
		SETENTITIES.clear();

	//limiter>
	auto weaponactive = playerpointer->m_aWeapons[playerpointer->m_nActiveWeaponSlot].m_nType;
	auto weaponclips = vector<pair<int, int>>();
	SAVEAPPLY
		//playerpointer->MakeChangesForNewWeapon(saveentity.id);
		//idk why but the above began causing the game to crash on a new setup with only the essentials pack, no idea what on the previous setup made it work so it stays disabled for now
		auto weaponid = eWeaponType(saveentity.id);
		auto weaponinfo = CWeaponInfo::GetWeaponInfo(weaponid, 1);
		auto weaponmodel1 = weaponinfo->m_nModelId1;
		auto weaponmodel2 = weaponinfo->m_nModelId2;
		if (weaponmodel1 > eModelID::MODEL_NULL) CStreaming::RequestModel(weaponmodel1, eStreamingFlags::PRIORITY_REQUEST);
		if (weaponmodel2 > eModelID::MODEL_NULL) CStreaming::RequestModel(weaponmodel2, eStreamingFlags::PRIORITY_REQUEST);
		CStreaming::LoadAllRequestedModels(false);
		playerpointer->GiveWeapon(weaponid, saveentity.ammo, false);
		if (weaponmodel1 > eModelID::MODEL_NULL) CStreaming::SetModelIsDeletable(weaponmodel1);
		if (weaponmodel2 > eModelID::MODEL_NULL) CStreaming::SetModelIsDeletable(weaponmodel2);
		auto weaponslot = weaponinfo->m_nSlot;
		if (saveentity.current) weaponactive = weaponid;
		weaponclips.push_back(make_pair(weaponslot, saveentity.clip));
	SAVEAPPLY2
	playerpointer->SetCurrentWeapon(weaponactive);
	for (auto weaponclip : weaponclips) playerpointer->m_aWeapons[weaponclip.first].m_nAmmoInClip = weaponclip.second;
	saveplayerweapons.clear();

	#undef SETENTITIES
	#define SETENTITIES savegangweapons
	SAVEAPPLY
		if (saveentity.gang < gangsamount) {
			patch::SETGANGSWEAPON(gangsWeapon(saveentity.gang, saveentity.weapon), saveentity.id, false);
		}
	SAVEAPPLY2

	#undef SETENTITIES
	#define SETENTITIES savepickupmodels
	SAVEAPPLY
		if (saveentity.pickup < pickupsamount) {
			patch::SETPICKUPSMODEL(pickupsModel(saveentity.pickup), saveentity.id, false);
			pickupsPointer(saveentity.pickup)->GetRidOfObjects();
		}
	SAVEAPPLY2

	#undef SETENTITIES
	#define SETENTITIES savegaragecars
	SAVEAPPLY
		if (saveentity.garage < garagesamount) {
			patch::SETGARAGESMODEL(garagesModel(saveentity.garage), saveentity.id, false);
		}
	SAVEAPPLY2

	#undef SETENTITIES
	#define SETENTITIES savegeneratorcars
	SAVEAPPLY
		if (saveentity.generator < generatorsamount) {
			patch::SETGENERATORSMODEL(generatorsModel(saveentity.generator), saveentity.id, false);
		}
	SAVEAPPLY2

	#undef SETENTITIES
	#define SETENTITIES savecarmods
	SAVEAPPLY
		if (
			saveentity.garage < garagesamount
			&& saveentity.mod < modsamount
			) {
			patch::SETMODSMODEL(modsModel(saveentity.mod, saveentity.garage), saveentity.id, false);
		}
	SAVEAPPLY2

	#undef SETENTITIES
	#define SETENTITIES saveclothmodels
	SAVEAPPLY
		CLOTHESMODELS[saveentity.part] = clothesHash(saveentity.name);
	SAVEAPPLY2
	CClothes::RebuildPlayerIfNeeded(playerpointer);

	#undef SETENTITIES
	#define SETENTITIES saveclothtextures
	SAVEAPPLY
		CLOTHESTEXTURES[saveentity.part] = clothesHash(saveentity.name);
	SAVEAPPLY2
	CClothes::RebuildPlayerIfNeeded(playerpointer);

	#undef SETENTITIES
	#define SETENTITIES saveblipsprites
	SAVEAPPLY
		if (saveentity.blip < blipsamount) {
			patch::SETBLIPSSPRITE(blipsSprite(saveentity.blip), saveentity.id, false);
		}
	SAVEAPPLY2

	if (saveplayercombo.id > 0) {
		PLAYERCOMBO = saveplayercombo.id;
	}
	saveplayercombo.id = 0;
	//<limiter
}

bool match_wildcard(const char* pattern, const char* string) {
	bool allow_subdir = false;
	bool had_anydir_in_pattern = false;
	while (true)
	{
		switch (*pattern++)
		{
		case '\0':
			if (*string == '\0') return true;
			if ((*string == '/' || *string == '\\') && (*(string + 1) == '\0')) return true;
			return false;

		case '/':
		case '\\':
			if (*string != '/' && *string != '\\')
			{
				if (*string == '\0' && *pattern == '\0')
					break;
				return false;
			}
			++string;
			had_anydir_in_pattern = true;
			break;

		case '*':
			allow_subdir = (!had_anydir_in_pattern || (*pattern == '*'));
			while (*pattern == '*') ++pattern;

			if (*pattern == '\0')
				return true;


			for (; *string; ++string)
			{
				if (match_wildcard(string, pattern))
					return true;
				else if ((*string == '/' || *string == '\\') && !allow_subdir)
					return false;
			}

			return false;

		case '?':
			if ((*string == '/' || *string == '\\') || *string == '\0')
				return false;
			++string;
			break;

		default:
			if (*string == '\0' || tolower(*string) != tolower(*(pattern - 1)))
				return false;
			++string;
			break;
		}
	}
}
//<

class AutoID3000 {
public:
    AutoID3000() {
        // Initialise your plugin here
        
//>
		auto timerstart = chrono::high_resolution_clock::now();

		if (loadedml.isinstalled) {
			auto autoid3000ml = fs::path("AutoID3000ML").replace_extension(".dll");
			copyWait(foldermod / autoid3000ml, folderml / fs::path(".data") / fs::path("plugins") / fs::path("gta3") / autoid3000ml);
		}

		logmain.writeText("Version: " + to_string(modversion));
		logmain.newLine();

		auto uninstallfile = fstream();
		uninstallfile.open(foldermod / fs::path("uninstall").replace_extension(".bat"), fstream::out);
		{
			uninstallfile <<
			R"(
			@echo off

			for /f %%i in ('dir /b "%~dp0backups"') do (
				goto notEmpty
			)
			goto isEmpty

			:notEmpty
				echo Not recommended to uninstall because:
				echo    1. Backups exist. Launch the game and manage them before trying again.
			goto exitScript

			:isEmpty
				echo Uninstall freely:
				echo    1. Go to the game directory.
				echo    2. Delete the file "#AutoID3000.asi".
				echo    3. Optional: Delete the folder "AutoID3000".
				echo    4. Optional: If Mod Loader is installed, go to "modloader/.data/plugins/gta3" and delete the files "AutoID3000ML.dll" and "AutoID3000ML.log".
			goto exitScript

			:exitScript
				echo[
				pause
			exit
			)"
			;
		}
		uninstallfile.close();

		auto storagecurrent = time(0);
		auto storagetime = *localtime(&storagecurrent);
		stringstream storagestream;
		storagestream << put_time(&storagetime, "%Y-%m-%d-%H-%M-%S");
		storagebackups = folderstorage / fs::path(storagestream.str());

		auto loadedLog = [](LoadedModule module, string name) {
			auto logtext = name;
			if (!module.isinstalled) logtext += " not";
			logmain.writeText(logtext + " installed.");
		};
		loadedLog(loadedml, "Mod Loader");
		loadedLog(loadedfla, MODULEFLA);
		loadedLog(loadedola, MODULEOLA);
		logmain.newLine();
		filesAll(folderroot, [](fs::path entrypath, fs::recursive_directory_iterator searchhandle) {
			if (fs::is_directory(entrypath)) {
				auto pathstring = lowerString((entrypath / fs::path()).string());
				if (
					pathstring == lowermod
					|| pathstring == lowerml
					) {
					searchhandle.disable_recursion_pending();
				}
			}
			else {
				auto &filepath = entrypath;
				auto fileextension = lowerString(filepath.extension().string());
				if (fileextension == EXTENSIONAUID3) filesauid3.insert(filepath);
			}
		});

		auto filesgtadat = set<fs::path>();
		if (loadedml.isinstalled) {
			{
				//filter 'filesml' by replicating Mod Loader behaviour: DONE
				struct MLCMD { //stores mod loader commandline data
					bool none = false; //'-nomods'
					set<string> mods; //list of mods passed through '-mod "mod 1"', '-mod "mod 2"' and so on
					string profile = "default"; //name of profile passed through '-modprof "name"', only the last one is used
				};
				auto parseCommandLine = []() { //parses the commandline, taken with slight modifications from https://github.com/thelink2012/modloader/blob/18f85c2766d4e052a452c7b1d8f5860a6daac24b/src/core/config.cpp#L74
					auto modlist = MLCMD();
					char buf[512];
					wchar_t **argv; int argc;
					argv = CommandLineToArgvW(GetCommandLineW(), &argc);
					if (argv) {
						auto toASCII = [](const wchar_t* wstr, char* abuf, size_t asize) {
							return !!WideCharToMultiByte(CP_ACP, 0, wstr, -1, abuf, asize, NULL, NULL);
						};
						std::string modprof;
						bool has_nomods_cmd = false;
						bool has_mod_cmd = false;
						bool has_modprof_cmd = false;
						for (int i = 0; i < argc; ++i) {
							if (argv[i][0] == '-') {
								wchar_t *arg = (i + 1 < argc ? argv[i + 1] : nullptr);
								wchar_t *argname = &argv[i][1];
								if (!_wcsicmp(argname, L"nomods")) has_nomods_cmd = true;
								else if (!_wcsicmp(argname, L"mod")) {
									if (arg == nullptr) break;
									else if (toASCII(arg, buf, sizeof(buf))) {
										modlist.mods.insert(buf);
										has_mod_cmd = true;
									}
								}
								else if (!_wcsicmp(argname, L"modprof")) {
									if (arg == nullptr) break;
									else if (toASCII(arg, buf, sizeof(buf))) {
										modprof = buf;
										has_modprof_cmd = true;
									}
								}
							}
						}
						if (has_nomods_cmd) modlist.none = true;
						else if (has_modprof_cmd) modlist.profile = lowerString(buf);
						LocalFree(argv);
					}
					return modlist;
				};
				auto cmdline = parseCommandLine();
				if (cmdline.none) filesml.clear(); //if commandline has '-nomods' then return no files
				else {
					struct MLProfile { //stores a mod loader profile
						string name = ""; //[Folder.Config] -> Profile
						string parent = ""; //[Profiles.Name.Config] -> Parents
						bool ignore = false; //[Profiles.Name.Config] -> IgnoreAllMods
						bool exclude = false; //[Profiles.Name.Config] -> ExcludeAllMods
						string module = ""; //[Profiles.Name.Config] -> UseIfModule
						set<string> files; //[Profiles.Name.IgnoreFiles]
						set<string> ignored; //[Profiles.Name.IgnoreMods]
						set<string> included; //[Profiles.Name.IncludeMods]
						set<string> exclusive; //[Profiles.Name.ExclusiveMods]
						auto print() {
							logmain.writeText("name: " + name);
							logmain.writeText("parent: " + parent);
							logmain.writeText("ignore: " + to_string(ignore));
							logmain.writeText("exclude: " + to_string(exclude));
							logmain.writeText("module: " + module);
							logmain.writeText("files:"); for (auto s : files) logmain.writeText("\t" + s);
							logmain.writeText("ignored:"); for (auto s : ignored) logmain.writeText("\t" + s);
							logmain.writeText("included:"); for (auto s : included) logmain.writeText("\t" + s);
							logmain.writeText("exclusive:"); for (auto s : exclusive) logmain.writeText("\t" + s);
						}
					};
					auto mlprofs = vector<MLProfile>(); //a vector containing all profiles, useful for finding one during e.g. inheritance
					auto readProfile = [&mlprofs](fs::path profpath) { //read an ini file to an MLProfile and put it in the MLProfile vector above
						auto proffile = fstream(); proffile.open(profpath);
						if (proffile.is_open()) {
							auto profile = MLProfile();
							auto profstream = stringstream(); profstream << proffile.rdbuf();
							auto profstring = lowerString(profstream.str()); profstream = stringstream(); profstream << profstring;
							auto profstart = profstream.tellg();
							auto profini = CIniReader(profstream);
							auto profname = profini.ReadString("folder.config", "profile", "");
							if (profname.length() > 0) {
								profile.name = profname;
								auto sectionconfig = string("profiles." + profname + ".config"); //profiles.name.section
								profile.parent = profini.ReadString(sectionconfig, "parents", "$none"); //default "$none"
								profile.ignore = profini.ReadBoolean(sectionconfig, "ignoreallmods", false);
								profile.exclude = profini.ReadBoolean(sectionconfig, "excludeallmods", false);
								profile.module = profini.ReadString(sectionconfig, "useifmodule", "");
								auto linelower = string();
								auto currentsection = int(0);
								auto inisections = map<string, vector<string>>();
								auto matchsection = regex(R"(^\s*\[\s*(.+?)\s*\].*)");
								auto matchname = regex(R"(^profiles\.(.+?)\.(\w+))"); //profiles.name.section
								auto matchkey = regex(R"(^\s*(.+?)\s*=\s*[+-]*0+[Xx\.]*0*)"); //key = 0
								auto resultsection = smatch();
								auto resultname = resultsection;
								auto resultkey = resultsection;
								profstream.clear(); profstream.seekg(profstart);
								while (getline(profstream, linelower)) {
									if (regex_match(linelower, resultsection, matchsection)) {
										currentsection = 0;
										auto resultstring = resultsection.str(1);
										if (
											regex_match(resultstring, resultname, matchname)
											&& resultname.str(1) == profname
											) {
											auto matchstring = resultname.str(2);
											if (matchstring == string("ignorefiles")) currentsection = 1; //corresponds to the case below
											else if (matchstring == string("ignoremods")) currentsection = 2;
											else if (matchstring == string("includemods")) currentsection = 3;
											else if (matchstring == string("exclusivemods")) currentsection = 4;
											else if (matchstring == string("priority")) currentsection = 5;
											continue;
										}
									}
									if (
										currentsection != 0
										&& linelower.length() > 0
										) {
										switch (currentsection) { //corresponds to the if-else-if above
										case 1: profile.files.insert(linelower);
											break;
										case 2: profile.ignored.insert(linelower);
											break;
										case 3: profile.included.insert(linelower);
											break;
										case 4:	profile.exclusive.insert(linelower);
											break;
										case 5:
											if (regex_match(linelower, resultkey, matchkey)) profile.ignored.insert(resultkey.str(1)); //if priority is 0, then add this mod to this profiles' list of ignored mods
											break;
										}
									}
								}
							}
							proffile.close();
							mlprofs.push_back(profile);
						}
					};
					auto defpath = folderml / fs::path("modloader.ini"); //read the file "modloader/modloader.ini" into an MLProfile
					if (fs::exists(defpath)) readProfile(defpath);
					else readProfile(folderml / fs::path(".data") / fs::path("modloader.ini.0")); //if the file above is not found, then read the file "modloader/.data/modloader.ini.0" into an MLProfile, since mod loader will copy it to the location of the above file anyway
					auto profspath = folderml / fs::path(".profiles"); //read all the iles inside "modloader/.profiles/" into MLProfiles
					if (fs::exists(profspath)) {
						for (auto profitem : fs::directory_iterator(profspath)) readProfile(profitem);
					}
					auto exclusivemods = set<string>(); //a global set storing exclusive mods of all profiles except current
					for (auto mlprof : mlprofs) {
						if (mlprof.name != cmdline.profile) exclusivemods.merge(mlprof.exclusive);
					}
					auto findProfile = [](vector<MLProfile> profs, string name) { //finds an MLProfile given its name
						for (auto prof : profs) {
							if (prof.name == name) return prof;
						}
						return profs[0];
					};
					auto currentprof = findProfile(mlprofs, cmdline.profile); //get the current profile, named "default" if commandline did not have any
					if (currentprof.ignore) filesml.clear(); //if [Profiles.Name.Config] -> IgnoreAllMods is true then return no files
					else {
						auto foldersml = set<string>(); //list of folders inside "modloader/" to scan
						auto mlout = (casePath(folderml) / fs::path()).string();
						if (cmdline.mods.size() > 0) foldersml = cmdline.mods; //if found one or more '-mod "mod"' inside commandline, then only scan these folders
						else {
							auto parentprof = currentprof; //begin inheritance of the current profile
							while (parentprof.parent != "$none") { //stop inheritance if there aren't any more parents
								parentprof = findProfile(mlprofs, parentprof.parent); //keep getting the parent of each gotten profile until one that doesn't have any
								currentprof.files.merge(parentprof.files); //inherit [Profiles.Name.IgnoreFiles]
								currentprof.ignored.merge(parentprof.ignored); //inherit [Profiles.Name.IgnoreMods]
								currentprof.included.merge(parentprof.included); //inherit [Profiles.Name.IncludeMods]
								for (auto exclusive : parentprof.exclusive) exclusivemods.erase(exclusive); //if the parent has any exclusive mods, then remove them from the global list of exclusive mods
							}
							for (auto modprof : mlprofs) { //inherit [Profiles.Name.Config] -> UseIfModule
								if (modprof.module.length() > 0 && LoadedModule(modprof.module, "", false).isinstalled) {
									parentprof = modprof;
									do {
										currentprof.files.merge(parentprof.files);
										currentprof.ignored.merge(parentprof.ignored);
										currentprof.included.merge(parentprof.included);
										for (auto exclusive : parentprof.exclusive) exclusivemods.erase(exclusive);
										parentprof = findProfile(mlprofs, parentprof.parent);
									} while (parentprof.parent != "$none");
									break; //only the first one found
								}
							}
							currentprof.ignored.merge(exclusivemods); //make the current profile ignore all of the mods from the global list of exclusive mods

							if (fs::exists(folderml)) { //create a list of all the folders inside "modloader/"
								for (auto mlfolder : fs::directory_iterator(folderml)) {
									auto folderpath = mlfolder.path();
									if (
										fs::is_directory(folderpath)
										&& folderpath.stem().string()[0] != CHARML
										) {
										foldersml.insert(lowerString(OutputPath(folderpath, mlout, true)));
									}
								}
							}

							if (currentprof.exclude) foldersml = currentprof.included; //[Profiles.Name.Config] -> ExcludeAllMods is true, then set the list of folders to [Profiles.Name.IncludeMods]
							for (auto mod : currentprof.ignored) foldersml.erase(mod); //remove all the mods in [Profiles.Name.IgnoreMods] from the list of folders
						}

						for (auto mlmod : foldersml) { //scan all the folders inside "modloader/" recursively and create a list of all the files
							mlmod = (folderml / mlmod).string();
							filesAll(mlmod, [](fs::path entrypath, fs::recursive_directory_iterator searchhandle) {
								if (entrypath.stem().string()[0] == CHARML) {
									if (fs::is_directory(entrypath)) {
										searchhandle.disable_recursion_pending();
									}
								}
								else {
									filesml.insert(entrypath);
								}
							});
						}

						auto tempfiles = set<fs::path>();
						
						tempfiles = filesml; filesml.clear();
						for (auto f : tempfiles) { //some optimization for the filtering to be done below
							auto e = lowerString(f.extension().string());
							if (
								e == EXTENSIONAUID3
								|| e == EXTENSIONIDE
								|| e == EXTENSIONTXT
								) filesml.insert(f);
							else if (lowerString(f.filename().string()) == "gta.dat") filesgtadat.insert(f);
						}

						tempfiles = filesml; filesml.clear();
						auto addFile = [&mlout](MLProfile *currentprof, fs::path mlfile) {
							for (auto wildcard : currentprof->files) {
								//"match_wildcard" taken from https://github.com/thelink2012/modloader/blob/18f85c2766d4e052a452c7b1d8f5860a6daac24b/src/core/wildcard.cpp#L20
								if (match_wildcard(wildcard.c_str(), lowerString(OutputPath(mlfile, mlout, true)).c_str())) return;
							}
							filesml.insert(mlfile);
						};
						for (auto mlfile : tempfiles) addFile(&currentprof, mlfile); //filter the list of files based on [Profiles.Name.IgnoreFiles]

						tempfiles = filesml; filesml.clear(); for (auto mlfile : tempfiles) filesml.insert(casePath(mlfile));
					}
				}
			}

			for (auto filepath : filesml) {
				auto fileextension = lowerString(filepath.extension().string());
				if (fileextension == EXTENSIONAUID3) filesauid3.insert(filepath);
				else if (fileextension == EXTENSIONIDE) fileside.insert(filepath);
				else if (fileextension == EXTENSIONTXT && fs::file_size(filepath) < 61440) filestxt.insert(filepath);
				else if (lowerString(filepath.filename().string()) == "gta.dat") filesgtadat.insert(filepath);
			}
		}

		if (filesauid3.size() > 0) {
			for (auto t : filestxt) filesgtadat.insert(lowerString(t.filename().string()));
			filesgtadat.insert(filestxt.begin(), filestxt.end()); //create a list of all level lines
			auto gtadatline = string();
			auto idelines = set<fs::path>({
				//always read
				"data/peds.ide",
				"data/vehicles.ide",
				"data/veh_mods.ide"
			});
			for (auto txtpath: filesgtadat) {
				auto txtfile = fstream(); txtfile.open(txtpath);
				if (txtfile.is_open()) {
					while (getline(txtfile, gtadatline)) {
						auto linelower = lowerString(gtadatline);
						if (linelower.substr(0, 4) == "ide ") {
							idelines.insert(fs::path(linelower.substr(5)).filename());
						}
					}
					txtfile.close();
				}
			}
			auto readDat = [&idelines](fs::path datstem, fs::path datfolder = folderdata) {
				auto datfile = fstream();
				datfile.open(datfolder / datstem.replace_extension(EXTENSIONDAT));
				if (datfile.is_open()) {
					auto datline = string();
					while (getline(datfile, datline)) {
						auto linelower = lowerString(datline);
						if (
							datline.length() > 4
							&& linelower.substr(0, 4) == "ide "
							) {
							auto idepath = folderroot / fs::path(datline.substr(4));
							fileside.insert(idepath);
							idelines.insert(fs::path(linelower).filename());
						}
					}
					datfile.close();
				}
			};
			readDat("default");
			readDat("gta");
			auto tempfiles = fileside; fileside.clear(); //remove ide files that don't have a level line
			for (auto idepath : tempfiles) {
				if (idelines.find(lowerString(idepath.filename().string())) != idelines.end()) fileside.insert(idepath);
				else logignored.writeText("Ignored IDE file " + OutputPath(idepath) + " because of missing level line.");
			}

			auto deleteGenerated = []() {
				for (auto filepath : filesauid3) {
					fs::remove(filepath.replace_extension(EXTENSIONIDE));
					fs::remove(filepath.replace_extension(EXTENSIONIPL));
					fs::remove(filepath.replace_extension(EXTENSIONTXT));
				}
			};
			deleteGenerated();
			Events::shutdownRwEvent += deleteGenerated;

			for (auto f : fileside) { if (!fs::exists(f)) fileside.erase(f); }
			for (auto f : filestxt) { if (!fs::exists(f)) filestxt.erase(f); }

			#define LOGEXTENSION "AUID3"
			#define LOGFILES filesauid3
			#define LOGREAD logfiles.writeText("Read " + string(LOGEXTENSION) + " files: " + to_string(LOGFILES.size())); for (auto filepath : LOGFILES) {
			#define LOGLINE logfiles.newLine();
			LOGREAD
				auid3names.insert(filepath.stem().string());

				Gfilesection = false;
				Gfileskip = false;
				Glinenumber = 0;

				Gauid3current = fs::path();
				Gauid3path = filepath;
				Gauid3fla = false;
				linesFile(filepath, true, [](string fileline, string linelower) {
					++Glinenumber;
					//limiter>
					if (linelower == SECTIONEND) Gfilesection = false;
					else if (linelower == SECTIONDEF) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONDEF, Gauid3fla = false;
					else if (linelower == SECTIONIDE) Gfilesection = true, Gfileskip = true, Gauid3current = Gauid3path, Gauid3current.replace_extension(EXTENSIONIDE), Gauid3fla = false;
					else if (linelower == SECTIONIPL) Gfilesection = true, Gfileskip = true, Gauid3current = Gauid3path, Gauid3current.replace_extension(EXTENSIONIPL), Gauid3fla = false;
					else if (linelower == SECTIONTXT) Gfilesection = true, Gfileskip = true, Gauid3current = Gauid3path, Gauid3current.replace_extension(EXTENSIONTXT), Gauid3fla = false;
					else if (
						linelower.length() > 5
						&& linelower.substr(0, 5) == SECTIONFIL
						) {
						Gfilesection = true;
						Gfileskip = true;
						Gauid3current = Gauid3path.parent_path() / fileline.substr(5);
						Gauid3fla = false;
						auto fileextension = lowerString(Gauid3current.extension().string());
						if (
							fileextension == EXTENSIONIDE
							|| fileextension == EXTENSIONIPL
							|| fileextension == EXTENSIONTXT
							) {
							Gfilesection = false;
							logignored.writeText("Ignored " + modname + " section \"" + fileline + "\" in file \"" + OutputPath(Gauid3path) + "\" at line " + to_string(Glinenumber) + " because of unsupported extension.");
						}
					}
					else if (linelower == SECTIONFLA) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONFLA, Gauid3fla = true;
					else if (linelower == SECTIONFLB) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONFLB, Gauid3fla = true;
					else if (linelower == SECTIONFLC) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONFLC, Gauid3fla = true;
					else if (linelower == SECTIONWPN) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONWPN, Gauid3fla = true;
					else if (linelower == SECTIONFLS) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONFLS, Gauid3fla = true;
					else if (linelower == SECTIONSPC) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONSPC, Gauid3fla = true;
					else if (linelower == SECTIONBLP) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONBLP, Gauid3fla = true;
					else if (linelower == SECTIONBLT) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONBLT, Gauid3fla = true;
					else if (linelower == SECTIONMEL) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONMEL, Gauid3fla = true;
					else if (linelower == SECTIONTRN) Gfilesection = true, Gfileskip = true, Gauid3current = SECTIONTRN, Gauid3fla = true;
					//<limiter

					if (
						Gfilesection
						&& !Gfileskip
						) {
						if (lowerString(Gauid3current.extension().string()) == EXTENSIONIDE) lineside.push_back(fileline);

						auto auid3string = Gauid3current.string();
						//limiter>
						if (auid3string == SECTIONWPN) weaponScan(fileline);
						if (auid3string == SECTIONBLP) blipScan(fileline);
						if (auid3string == SECTIONBLT) fileline = (Gauid3path.parent_path() / fs::path(fileline)).string();
						if (auid3string == SECTIONMEL) comboScan(fileline);
						if (auid3string == SECTIONTRN) trainScan(fileline);
						//<limiter

						linesAdd(Gauid3current, fileline);
					}
					Gfileskip = false;
				});
			}
			LOGLINE

			AUID3Iterator(SECTIONDEF, true, [](string auid3line, string auid3lower) {
				auid3names.insert(auid3line);
			});

			auto sectionExists = [](string auid3section){
				auto sectionfind = linesfiles.find(auid3section);
				if (sectionfind != linesfiles.end()) return true;
				return false;
			};
			auto sectionsExist = [&sectionExists](vector<string> auid3sections) {
				for (auto auid3section : auid3sections) {
					if (sectionExists(auid3section)) return true;
				}
				return false;
			};

			auto arekillable = int(0);
			auto areped = arekillable;
			auto arevehicle = arekillable;
			auto arebike = arekillable;
			auto areflying = arekillable;
			auto arewater = arekillable;
			//limiter>
			auto areweapon = arekillable;
			auto arecloth = arekillable;
			auto areblip = arekillable;
			auto arecombo = arekillable;
			auto aretrain = arekillable;
			//<limiter
			auto aredamage = arekillable;
			auto aretimed = arekillable;
			auto arehier = arekillable;
			auto areobject = arekillable;
			auto areinfo = arekillable;
			auto arecol = arekillable;
			for (auto auid3name : auid3names) {
				auto firstchar = char(); if (auid3name.length() > 0) firstchar = auid3name[0];
				auto secondchar = char(); if (auid3name.length() > 1) secondchar = auid3name[1];
				auto thirdchar = char(); if (auid3name.length() > 2) thirdchar = auid3name[2];
				if (
					firstchar != CHARIGNORE
					&& secondchar != CHARIGNORE
					) {
					auto isped = firstchar == CHARPED;
					auto isvehicle = firstchar == CHARVEHICLE;
					if (isped || isvehicle) {
						++arekillable;
						if (isped) {
							++areped;
						}
						else if (isvehicle) {
							++arevehicle;
							if (secondchar == CHARBIKE) {
								++arebike;
							}
							else if (secondchar == CHARFLYING) {
								++areflying;
							}
							else if (secondchar == CHARWATER) {
								++arewater;
							}
						}
					}
					//limiter>
					else if (firstchar == CHARWEAPON) {
						++areweapon;
					}
					else if (firstchar == CHARCLOTH) {
						++arecloth;
					}
					else if (firstchar == CHARBLIP) {
						++areblip;
					}
					else if (firstchar == CHARCOMBO) {
						++arecombo;
					}
					else if (firstchar == CHARTRAIN) {
						++aretrain;
					}
					//<limiter
					else {
						if (firstchar == CHARDAMAGE) {
							++aredamage;
						}
						else if (firstchar == CHARTIMED) {
							++aretimed;
						}
						else if (firstchar == CHARHIER) {
							++arehier;
						}
						else {
							++areobject;
						}
						if (secondchar == CHARINFO) {
							++areinfo;
						}
						if (thirdchar != CHARCOL) {
							++arecol;
						}
					}
				}
			}
			if (arekillable > 0) {
				linesAdd(SECTIONFLC, "id limits:count of killable model ids:800:10000");
			}
			if (areped > 0) {
				linesAdd(SECTIONFLA, "ide limits:ped models:278:" + to_string(areped));
				linesAdd(SECTIONFLC, "ped streaming:pedgrp peds per group:21:210");
			}
			if (arevehicle > 0) {
				linesAdd(SECTIONFLA, "dynamic limits:vehiclestructs:50:2500");
				linesAdd(SECTIONFLA, "ide limits:vehicle models:212:" + to_string(arevehicle));
				linesAdd(SECTIONFLC, "car streaming:cargrp cars per group:23:63");
				linesAdd(SECTIONFLB, "car streaming:accept any id for car generator");
				linesAdd(SECTIONFLB, "handling.cfg limits:apply handling.cfg patch");
				linesAdd(SECTIONFLA, "handling.cfg limits:number of standard lines:210:" + to_string(arevehicle));
				linesAdd(SECTIONFLC, "other limits:vehicle colors:128:255");
				linesAdd(SECTIONFLB, "special:make paintjobs work for any id");
				linesAdd(SECTIONFLB, "addons:enable vehicle audio loader");
			}
			if (arebike > 0) {
				linesAdd(SECTIONFLA, "handling.cfg limits:number of bike lines:13:" + to_string(arebike));
			}
			if (areflying > 0) {
				linesAdd(SECTIONFLA, "handling.cfg limits:number of flying lines:24:" + to_string(areflying));
			}
			if (arewater > 0) {
				linesAdd(SECTIONFLA, "handling.cfg limits:number of boat lines:12:" + to_string(arewater));
			}
			//limiter>
			if (areweapon > 0) {
				linesAdd(SECTIONFLA, "ide limits:weapon models:51:" + to_string(areweapon));
				linesAdd(SECTIONFLB, "weapon limits:enable weapon type loader");
				linesAdd(SECTIONFLC, "weapon limits:weapon type loader, number of type ids:70:255");
			}
			if (arecloth > 0) {
				linesAdd(SECTIONFLA, "directory limits:clothes directory:550:" + to_string(arecloth));
			}
			if (areblip > 0) {
				linesAdd(SECTIONFLB, "addons:enable radar blip sprite filename loader");
				linesAdd(SECTIONFLC, "addons:radar blip sprite filename loader, number of type ids:64:255");
			}
			if (arecombo > 0) {
				linesAdd(SECTIONFLB, "weapon limits:enable melee combo type loader");
				linesAdd(SECTIONFLC, "weapon limits:max number of melee combos:17:255");
			}
			if (aretrain > 0) {
				linesAdd(SECTIONFLB, "addons:enable train type carriages loader");
				linesAdd(SECTIONFLC, "addons:train type carriage loader, max number of vehicles for type:15:255");
				linesAdd(SECTIONFLC, "addons:train type carriage loader, number of type ids:16:255");
			}
			//<limiter
			if (aredamage > 0) {
				linesAdd(SECTIONFLA, "ide limits:ide objects type 2:70:" + to_string(aredamage));
			}
			if (aretimed > 0) {
				linesAdd(SECTIONFLA, "ide limits:timed objects:169:" + to_string(aretimed));
			}
			if (arehier > 0) {
				linesAdd(SECTIONFLA, "ide limits:hier objects:92:" + to_string(arehier));
			}
			if (areobject > 0) {
				linesAdd(SECTIONFLA, "ide limits:ide objects type 1:14000:" + to_string(areobject));
			}
			if (areinfo > 0) {
				linesAdd(SECTIONFLA, "other limits:object info entries:160:" + to_string(areinfo));
			}
			if (arecol > 0) {
				linesAdd(SECTIONFLA, "dynamic limits:colmodels:10150:" + to_string(arecol));
			}

			//limiter>
			#define EXISTSFLABC sectionsExist({ SECTIONFLA, SECTIONFLB, SECTIONFLC })
			#define EXISTSWPN sectionExists(SECTIONWPN)
			#define EXISTSFLS sectionExists(SECTIONFLS)
			#define EXISTSSPC sectionExists(SECTIONSPC)
			#define EXISTSBLP sectionExists(SECTIONBLP)
			#define EXISTSMEL sectionExists(SECTIONMEL)
			#define EXISTSTRN sectionExists(SECTIONTRN)
			//<limiter

			//limiter>
			if (loadedfla.isinstalled) {
				backupflaconfig.initializeBackup();
				backupflaweapon.initializeBackup();
				backupflaaudio.initializeBackup();
				backupflamodel.initializeBackup();
				backupflablip.initializeBackup();
				backupflacombo.initializeBackup();
				backupflatrain.initializeBackup();
			}
			//<limiter

			if (
				(!loadedfla.isinstalled
					|| (
						//limiter>
							backupflaconfig.warnBackup()
						&&	backupflaweapon.warnBackup()
						&&	backupflaaudio.warnBackup()
						&&	backupflamodel.warnBackup()
						&&	backupflablip.warnBackup()
						&&	backupflacombo.warnBackup()
						&&	backupflatrain.warnBackup()
						//<limiter
						)
					)
				//&& 
				) {

			if (
				!loadedfla.isinstalled
				&& Gauid3fla
				) {
				if (modMessage("Current " + modname + " configuration requires " + MODULEFLA + " which could not be detected. Would you like to close the game? It will probably crash anyway.", MB_YESNO | MB_ICONWARNING) == IDYES) {
					exit(EXIT_SUCCESS);
				}
			}

			if (loadedfla.isinstalled) {
				//limiter>
				if (EXISTSFLABC) {
					backupflaconfig.createBackup();
					Events::attachRwPluginsEvent += [] {backupflaconfig.restoreBackup(); };
				}

				if (EXISTSWPN) {
					backupflaweapon.createBackup();
					Events::attachRwPluginsEvent += [] {backupflaweapon.restoreBackup(); };
				}

				if (EXISTSFLS) {
					Events::initPoolsEvent.before += [] {backupflaaudio.createBackup(); };
					Events::initGameEvent.after += [] {backupflaaudio.restoreBackup(); };
				}

				if (EXISTSSPC) {
					Events::initPoolsEvent.before += [] {backupflamodel.createBackup(); };
					Events::initGameEvent.after += [] {backupflamodel.restoreBackup(); };
				}

				if (EXISTSBLP) {
					backupflablip.createBackup();
					Events::attachRwPluginsEvent += [] {backupflablip.restoreBackup(); };
				}

				if (EXISTSMEL) {
					backupflacombo.createBackup();
					Events::attachRwPluginsEvent += [] {backupflacombo.restoreBackup(); };
				}

				if (EXISTSTRN) {
					backupflatrain.createBackup();
					Events::attachRwPluginsEvent += [] {backupflatrain.restoreBackup(); };
				}
				//<limiter
			}

			auto fillIDs = [](int startid, int endid, set<int> *idset) {for (auto currentid = startid; currentid <= endid; ++currentid) idset->insert(currentid); };
			//limiter>
			fillIDs(0, 10, &modelsused); fillIDs(374, 383, &modelsused); fillIDs(15000, 15024, &modelsused);
			fillIDs(0, 69, &weaponsused);
			fillIDs(0, 63, &blipsused);
			fillIDs(0, 16, &combosused);
			fillIDs(0, 15, &trainsused);
			//<limiter

			#undef LOGEXTENSION
			#define LOGEXTENSION "IDE"
			#undef LOGFILES
			#define LOGFILES fileside
			LOGREAD
				linesFile(filepath, true, [](string fileline, string linelower) {
					lineside.push_back(fileline);
				});
			}
			LOGLINE

			Gfilesection = false;
			Gfileskip = false;
			for (auto ideline : lineside) {
				auto linelower = lowerString(ideline);
				if (linelower == "end") Gfilesection = false;
				else if (
					linelower == "peds"
					|| linelower == "cars"
					|| linelower == "objs"
					|| linelower == "tobj"
					|| linelower == "hier"
					|| linelower == "anim"
					|| linelower == "weap"
					) {
					Gfilesection = true;
					Gfileskip = true;
				}

				if (
					Gfilesection
					&& !Gfileskip
					) {
					modelScan(ideline);
				}
				Gfileskip = false;
			}

			#undef LOGEXTENSION
			#define LOGEXTENSION "TXT"
			#undef LOGFILES
			#define LOGFILES filestxt
			LOGREAD
				linesFile(filepath, true, [](string fileline, string linelower) {
					modelScan(fileline);
				});
			}
			LOGLINE

			if (loadedfla.isinstalled) {
				if (EXISTSFLABC) {
					auto flalower = fstream();
					flalower.open(backupflaconfig.originalpath.string(), fstream::in);
					auto flabuffer = stringstream();
					flabuffer << flalower.rdbuf();
					flalower.close();
					auto flastring = lowerString(flabuffer.str());
					flalower.open(backupflaconfig.originalpath.string(), fstream::out);
					flalower << flastring;
					flalower.close();

					FLAApply();
				}

				auto flalimits = CIniReader(backupflaconfig.originalpath.string());
				limitkillables = flalimits.ReadInteger("id limits", "count of killable model ids", limitkillables);
				if (flalimits.ReadInteger("id limits", "apply id limit patch", 1) == 1) {
					limitmodels = flalimits.ReadInteger("id limits", "file_type_dff", limitmodels);
				}

				//limiter>
				if (flalimits.ReadInteger("weapon limits", "enable weapon type loader", 1) == 1) {
					limitweapons = flalimits.ReadInteger("weapon limits", "weapon type loader, number of type ids", limitweapons);
				}

				if (flalimits.ReadInteger("addons", "enable radar blip sprite filename loader", 1) == 1) {
					limitblips = flalimits.ReadInteger("addons", "radar blip sprite filename loader, number of type ids", limitblips);
				}

				if (flalimits.ReadInteger("weapon limits", "enable melee combo type loader", 1) == 1) {
					limitcombos = flalimits.ReadInteger("weapon limits", "max number of melee combos", limitcombos);
				}

				if (flalimits.ReadInteger("addons", "enable train type carriages loader", 1) == 1) {
					limittrains = flalimits.ReadInteger("addons", "train type carriage loader, number of type ids", limittrains);
				}
				//<limiter
			}

			limitkillables--;
			limitmodels--;
			//limiter>
			limitweapons--;
			limitblips--;
			limitcombos--;
			limittrains--;
			//<limiter

			#define MESSAGEFREE string("Run out of free ")

			#define MESSAGEID string(" ID")
			#define MESSAGEIDS MESSAGEID + string("s")

			#define MESSAGEMODEL string("model")
			#define MESSAGEKILLABLE string("killable") + " " + MESSAGEMODEL
			#define MESSAGEREGULAR string("regular") + " " + MESSAGEMODEL
			//limiter>
			#define MESSAGEWEAPON string("weapon")
			#define MESSAGEBLIP string("blip")
			#define MESSAGECOMBO string("combo")
			#define MESSAGETRAIN string("train")
			//<limiter

			#define MESSAGENAME MESSAGEKILLABLE
			#define MESSAGELIMIT limitkillables
			#define MESSAGEASSIGN logmain.writeText("Highest " + MESSAGENAME + MESSAGEID + ": " + to_string(MESSAGELIMIT));
			MESSAGEASSIGN

			#undef MESSAGENAME 
			#define MESSAGENAME MESSAGEREGULAR
			#undef MESSAGELIMIT 
			#define MESSAGELIMIT limitmodels
			MESSAGEASSIGN
			//limiter>
			#undef MESSAGENAME
			#define MESSAGENAME MESSAGEWEAPON
			#undef MESSAGELIMIT 
			#define MESSAGELIMIT limitweapons
			MESSAGEASSIGN

			#undef MESSAGENAME
			#define MESSAGENAME MESSAGEBLIP
			#undef MESSAGELIMIT 
			#define MESSAGELIMIT limitblips
			MESSAGEASSIGN

			#undef MESSAGENAME
			#define MESSAGENAME MESSAGECOMBO
			#undef MESSAGELIMIT 
			#define MESSAGELIMIT limitcombos
			MESSAGEASSIGN

			#undef MESSAGENAME
			#define MESSAGENAME MESSAGETRAIN
			#undef MESSAGELIMIT 
			#define MESSAGELIMIT limittrains
			MESSAGEASSIGN
			//<limiter

			logmain.newLine();

			for (auto modelid = int(0); modelid <= limitmodels; ++modelid) {
				if (modelsused.find(modelid) == modelsused.end()) {
					if (modelid <= limitkillables) {
						freekillables.insert(modelid);
					}
					else {
						freemodels.insert(modelid);
					}
				}
			}

			if (loadedfla.isinstalled) {
				//limiter>
				linesFile(backupflaweapon.originalpath, false, [](string fileline, string linelower) {
					weaponScan(fileline);
				});

				linesFile(backupflablip.originalpath, false, [](string fileline, string linelower) {
					blipScan(fileline);
				});

				linesFile(backupflacombo.originalpath, false, [](string fileline, string linelower) {
					comboScan(fileline);
				});

				linesFile(backupflatrain.originalpath, false, [](string fileline, string linelower) {
					trainScan(fileline);
				});
				//<limiter
			}

			auto usedIDs = [](int *endid, set<int> *idsused, set<int> *idsfree) {
				for (auto currentid = int(0); currentid <= *endid; ++currentid) {
					if (idsused->find(currentid) == idsused->end()) {
						idsfree->insert(currentid);
					}
				}
			};

			//limiter>
			usedIDs(&limitweapons, &weaponsused, &freeweapons);
			usedIDs(&limitblips, &blipsused, &freeblips);
			usedIDs(&limitcombos, &combosused, &freecombos);
			usedIDs(&limittrains, &trainsused, &freetrains);
			//<limiter

			auto indexmodels = int(0);
			auto indexkillables = indexmodels;
			//limiter>
			auto indexweapons = indexmodels;
			auto indexblips = indexmodels;
			auto indexcombos = indexmodels;
			auto indextrains = indexmodels;
			//<limiter

			auto maxmodels = freemodels.size();
			auto maxkillables = freekillables.size();
			//limiter>
			auto maxweapons = freeweapons.size();
			auto maxblips = freeblips.size();
			auto maxcombos = freecombos.size();
			auto maxtrains = freetrains.size();
			//<limiter

			auto messagemodels = bool(false);
			//limiter>
			auto messageweapons = messagemodels;
			auto messageblips = messagemodels;
			auto messagecombos = messagemodels;
			auto messagetrains = messagemodels;
			//<limiter

			#define ASSIGNSTOP + "."
			static auto assignError = [](string name, string auid3) {
				logassigned.writeText(MESSAGEFREE + name + MESSAGEIDS + " to assign any to " + auid3 ASSIGNSTOP);
			};
			auto assignID = [](int *index, size_t max, set<int> free, string auid3, string name, bool forced, bool *message = nullptr) {
				if (*index < max) {
					auto assignedid = *next(free.begin(), *index);
					auid3sids[auid3] = assignedid;
					++*index;
					logassigned.writeText("Assigned " + name + MESSAGEID + " " + to_string(assignedid) + " to " + auid3 + (forced ? " (forced)" : "") ASSIGNSTOP);
					return true;
				}
				else {
					if (message != nullptr) {
						assignError(name, auid3);
						*message = true;
					}
				}
				return false;
			};

			for (auto auid3name : auid3names) {
				auto firstchar = char(); if (auid3name.length() > 0) firstchar = auid3name[0];
				if (firstchar != CHARIGNORE) {
					auto isped = firstchar == CHARPED;
					auto isvehicle = firstchar == CHARVEHICLE;
					if (isped || isvehicle) {
						#define FORCEFIRST false
						#define FORCESECOND true
						#define ASSIGNKILLABLE if (assignID(&indexkillables, maxkillables, freekillables, auid3name, MESSAGEKILLABLE, FORCEFIRST));
						#define ASSIGNREGULAR if (assignID(&indexmodels, maxmodels, freemodels, auid3name, MESSAGEREGULAR, FORCESECOND));
						#define ASSIGNNONE else { assignError(MESSAGEMODEL, auid3name); messagemodels = true; }

						ASSIGNKILLABLE
						else ASSIGNREGULAR
						ASSIGNNONE
					}
					//limiter>
					else if (firstchar == CHARWEAPON) {
						assignID(&indexweapons, maxweapons, freeweapons, auid3name, MESSAGEWEAPON, false, &messageweapons);
					}
					else if (firstchar == CHARCLOTH) {
						auid3sids[auid3name] = 0;
						logassigned.writeText("Assigned " + auid3name + " as cloth.");
					}
					else if (firstchar == CHARBLIP) {
						assignID(&indexblips, maxblips, freeblips, auid3name, MESSAGEBLIP, false, &messageblips);
					}
					else if (firstchar == CHARCOMBO) {
						assignID(&indexcombos, maxcombos, freecombos, auid3name, MESSAGECOMBO, false, &messagecombos);
					}
					else if (firstchar == CHARTRAIN) {
						assignID(&indextrains, maxtrains, freetrains, auid3name, MESSAGETRAIN, false, &messagetrains);
					}
					//<limiter
					else {
						#undef FORCEFIRST
						#define FORCEFIRST true
						#undef FORCESECOND
						#define FORCESECOND false
						ASSIGNREGULAR
						else ASSIGNKILLABLE
						ASSIGNNONE
					}
				}
			}

			for (auto auid3data : auid3sids) {
				auto auid3string = auid3data.first.string();
				auto auid3id = auid3data.second;
				//limiter>
				if (auid3string[0] == CHARWEAPON) weaponsauid3s[auid3id] = auid3string;
				else if (auid3string[0] == CHARCLOTH) clothesauid3s[clothesHash(auid3string)] = auid3string;
				else if (auid3string[0] == CHARBLIP) blipsauid3s[auid3id] = auid3string;
				else if (auid3string[0] == CHARCOMBO) combosauid3s[auid3id] = auid3string;
				else if (auid3string[0] == CHARTRAIN) trainsauid3s[auid3id] = auid3string;
				//<limiter
				else idsauid3s[auid3id] = auid3string;
			}

			auto logFree = [](string logname, set<int> *freeids, map<int, string> *auid3id) {
				logfree.writeText("Free " + logname + MESSAGEIDS +  ": " + to_string(freeids->size()));
				for (auto freeid : *freeids) {
					logfree.writeText(LOGINDENT to_string(freeid) + (auid3id->find(freeid) != auid3id->end() ? " (used by " + modname + ")" : ""));
				}
				logfree.newLine();
			};

			logFree(MESSAGEKILLABLE, &freekillables, &idsauid3s);
			logFree(MESSAGEREGULAR, &freemodels, &idsauid3s);
			//limiter>
			logFree(MESSAGEWEAPON, &freeweapons, &weaponsauid3s);
			logFree(MESSAGEBLIP, &freeblips, &blipsauid3s);
			logFree(MESSAGECOMBO, &freecombos, &combosauid3s);
			logFree(MESSAGETRAIN, &freetrains, &trainsauid3s);
			//<limiter

			auto messageunassigned = bool(false);
			for (auto &filepath : linesfiles) {
				Glinenumber = 0;
				for (auto &fileline : filepath.second) {
					++Glinenumber;
					while (true) {
						auto separatorstart = fileline.find_first_of('<');
						auto separatorend = fileline.find_first_of('>');
						if (
							separatorstart == string::npos
							|| separatorend == string::npos
							) {
							break;
						}
						else if (separatorend > separatorstart) {
							auto auid3name = fileline.substr(0, separatorend);
							auid3name = auid3name.substr(separatorstart + 1);

							auto defaultid = int(-1);

							auto silentmode = bool(false);
							auto silentseparator = auid3name.find_last_of('*');
							if (silentseparator != string::npos) {
								auto silentstring = auid3name.substr(silentseparator + 1);
								auto silentid = int();
								if (sscanf(silentstring.c_str(), "%d", &silentid) == 1) {
									defaultid = silentid;
								}

								auid3name = auid3name.substr(0, silentseparator);

								silentmode = true;
							}

							auto assignedid = int();

							auto auid3found = auid3sids.find(auid3name);
							if (auid3found != auid3sids.end()) {
								assignedid = auid3found->second;
							}
							else {
								assignedid = defaultid;
								if (!silentmode) messageunassigned = true;
								logunassigned.writeText(auid3name + " in file \"" + OutputPath(filepath.first) + " at line " + to_string(Glinenumber) + ".");
							}

							fileline = fileline.substr(0, separatorstart) + to_string(assignedid) + fileline.substr(separatorend + 1);
						}
					}
				}
			}

			if (loadedfla.isinstalled) {
				//limiter>
				if (EXISTSWPN) {
					FLAWrite(backupflaweapon.originalpath, SECTIONWPN, true);
				}

				if (EXISTSFLS) {
					Events::initPoolsEvent.after += [] {
						auto flslines = vector<string>();
						Gflafile.open(backupflaaudio.originalpath, fstream::in);
						if (Gflafile.is_open()) {
							auto flsline = string();
							while (getline(Gflafile, flsline)) {
								if (lowerString(flsline) == ";the end") break;
								if (flsline.length() > 0) flslines.push_back(flsline);
							}
							Gflafile.close();
						}
						auto flsfind = linesfiles.find(SECTIONFLS);
						if (flsfind != linesfiles.end()) {
							for (auto flsline : flsfind->second) if (flsline.length() > 0) flslines.push_back(flsline);
							flsfind->second.clear();
						}
						flslines.push_back(";the end");
						Gflafile.open(backupflaaudio.originalpath, fstream::out);
						for (auto flsline : flslines) Gflafile << flsline << '\n';
						Gflafile.close();
					};
				}

				if (EXISTSSPC) {
					Events::initPoolsEvent.after += [] { FLAWrite(backupflamodel.originalpath, SECTIONSPC, true); };
				}

				if (EXISTSBLP) {
					FLAWrite(backupflablip.originalpath, SECTIONBLP, true);
				}

				Events::initGameEvent += [] {
					static auto blipssprites = patch::Get<CSprite2d *>(0x5827EB, true);
					AUID3Iterator(SECTIONBLT, true, [](string auid3line, string auid3lower) {
						auto blipseparator = auid3line.find_first_of(';');
						if (blipseparator != string::npos) {
							auto txdpath = fs::path(auid3line.substr(0, blipseparator));
							auid3line = auid3line.substr(blipseparator + 1);
							blipseparator = auid3line.find_first_of(';');
							if (blipseparator != string::npos) {
								auto texturename = auid3line.substr(0, blipseparator);
								auto blipstring = auid3line.substr(blipseparator + 1);
								auto blipid = int();
								if (sscanf(blipstring.c_str(), "%d", &blipid) == 1) {
									auto txdpointer = CFileLoader::LoadTexDictionary(txdpath.string().c_str());
									auto texturepointer = RwTexDictionaryFindNamedTexture(txdpointer, texturename.c_str());
									blipssprites[blipid].m_pTexture = texturepointer;
								}
							}
						}
					});
				};

				if (EXISTSMEL) {
					FLAWrite(backupflacombo.originalpath, SECTIONMEL, true);
				}

				if (EXISTSTRN) {
					FLAWrite(backupflatrain.originalpath, SECTIONTRN, true);
				}
				//<limiter
			}

			for (auto filepath : linesfiles) {
				if (filepath.second.size() > 0) {
					auto linesfile = fstream();

					//more trouble than it's worth
					//auto &currentlines = filepath.second;
					//auto &savedfile = linesfile; savedfile.open(filepath.first, fstream::in);
					//if (savedfile.is_open()) {
					//	auto currentstring = string(); for (auto currentline : currentlines) currentstring += currentline + '\n';
					//	auto currenthash = CRC::Calculate(currentstring.data(), currentstring.size(), CRC::CRC_32());
					//	auto savedbuffer = stringstream(); savedbuffer << savedfile.rdbuf();
					//	auto savedstring = savedbuffer.str();
					//	auto savedhash = CRC::Calculate(savedstring.data(), savedstring.size(), CRC::CRC_32());
					//	savedfile.close();
					//	if (currenthash = savedhash) continue;
					//}

					linesfile.open(filepath.first, fstream::out);
					if (linesfile.is_open()) {
						for (auto fileline : filepath.second) {
							linesfile << fileline + '\n';
						}
						linesfile.close();
					}
				}
			}

			CdeclEvent <AddressList< //CRopes::Shutdown
					0x618F51, H_CALL //CGenericGameStorage::DoGameSpecificStuffBeforeSave
			>, PRIORITY_BEFORE, ArgPickNone, void()> CRopes__ShutdownEvent;
			CRopes__ShutdownEvent += [] {
				savePointers();

				#define FINDMAP weaponsauid3s
				#define FINDAUID3 auto auid3find = FINDMAP.find(auid3id); \
					if (auid3find != FINDMAP.end()) { \
						auid3id = auid3find->first;

				//limiter>
				auto weaponslot = playerpointer->m_nActiveWeaponSlot;
				for (auto weapondata : playerpointer->m_aWeapons) {
					auto auid3id = int(weapondata.m_nType);
					FINDAUID3
						if (weapondata.m_nState != eWeaponState::WEAPONSTATE_OUT_OF_AMMO) {
							auto auid3weapon = eWeaponType(auid3id);
							auto saveentity = SavePlayerWeapon(
									auid3id
								,	auid3find->second
								,	weapondata.m_nTotalAmmo
								,	weaponslot == CWeaponInfo::GetWeaponInfo(auid3weapon, 1)->m_nSlot
								,	weapondata.m_nAmmoInClip
							);
							saveplayerweapons.push_back(saveentity);
							playerpointer->ClearWeapon(auid3weapon);
						}
					}
				}

				#define LOOPID entityid
				#define LOOPINDEX LOOPID, entityindex
				#define LOOPFILL , entityindex

				#define LOOPAMOUNT gangsamount
				#define LOOPPOINTER gangsWeapon
				#define LOOPGET GETGANGSWEAPON
				#define LOOPCALL LOOPINDEX
				#define LOOPCLASS SaveGangWeapon
				#define LOOPEXTEND LOOPFILL
				#define LOOPSET SETGANGSWEAPON
				#define LOOPCLEAR eWeaponType::WEAPON_UNARMED
				#define LOOPLIST savegangweapons
				#define LOOPFUNCTION for (auto entityid = POINTERSSTART(0); entityid < LOOPAMOUNT; ++entityid) { \
					auto entitypointer = LOOPPOINTER(LOOPCALL); \
					auto auid3id = patch::LOOPGET(entitypointer, false); \
					FINDAUID3 \
						auto saveentity = LOOPCLASS( \
								auid3id \
							,	auid3find->second \
							,	entityid \
							LOOPEXTEND \
						); \
						patch::LOOPSET(entitypointer, LOOPCLEAR, false); \
						LOOPLIST.push_back(saveentity); \
					} \
				}
				#define INDEXAMOUNT 3
				#define INDEXFUNCTION for (auto entityindex = POINTERSSTART(0); entityindex < INDEXAMOUNT; ++entityindex)
				INDEXFUNCTION LOOPFUNCTION

				#undef FINDMAP
				#define FINDMAP idsauid3s

				CPickups::RemoveUnnecessaryPickups(CVector(0.0f, 0.0f, 0.0f), 99999.0f);
				#undef LOOPAMOUNT
				#define LOOPAMOUNT pickupsamount
				#undef LOOPPOINTER
				#define LOOPPOINTER pickupsModel
				#undef LOOPGET
				#define LOOPGET GETPICKUPSMODEL
				#undef LOOPCALL
				#define LOOPCALL LOOPID
				#undef LOOPCLASS
				#define LOOPCLASS SavePickupModel
				#undef LOOPEXTEND
				#define LOOPEXTEND 
				#undef LOOPSET
				#define LOOPSET SETPICKUPSMODEL
				#undef LOOPCLEAR
				#define LOOPCLEAR eModelID::MODEL_INFO
				#undef LOOPLIST
				#define LOOPLIST savepickupmodels
				LOOPFUNCTION

				#undef LOOPAMOUNT
				#define LOOPAMOUNT garagesamount
				#undef LOOPPOINTER
				#define LOOPPOINTER garagesModel
				#undef LOOPGET
				#define LOOPGET GETGARAGESMODEL
				#undef LOOPCALL
				#define LOOPCALL LOOPID
				#undef LOOPCLASS
				#define LOOPCLASS SaveGarageCar
				#undef LOOPEXTEND
				#define LOOPEXTEND 
				#undef LOOPSET
				#define LOOPSET SETGARAGESMODEL
				#undef LOOPCLEAR
				#define LOOPCLEAR eModelID::MODEL_NULL
				#undef LOOPLIST
				#define LOOPLIST savegaragecars
				LOOPFUNCTION

				#undef LOOPAMOUNT
				#define LOOPAMOUNT generatorsamount
				#undef LOOPPOINTER
				#define LOOPPOINTER generatorsModel
				#undef LOOPGET
				#define LOOPGET GETGENERATORSMODEL
				#undef LOOPCALL
				#define LOOPCALL LOOPID
				#undef LOOPCLASS
				#define LOOPCLASS SaveGeneratorCar
				#undef LOOPEXTEND
				#define LOOPEXTEND 
				#undef LOOPSET
				#define LOOPSET SETGENERATORSMODEL
				#undef LOOPCLEAR
				#define LOOPCLEAR -1
				#undef LOOPLIST
				#define LOOPLIST savegeneratorcars
				LOOPFUNCTION

				#undef LOOPAMOUNT
				#define LOOPAMOUNT modsamount
				#undef LOOPPOINTER
				#define LOOPPOINTER modsModel
				#undef LOOPGET
				#define LOOPGET GETMODSMODEL
				#undef LOOPCALL
				#define LOOPCALL LOOPINDEX
				#undef LOOPCLASS
				#define LOOPCLASS SaveCarMod
				#undef LOOPEXTEND
				#define LOOPEXTEND LOOPFILL
				#undef LOOPSET
				#define LOOPSET SETMODSMODEL
				#undef LOOPCLEAR
				#define LOOPCLEAR -1
				#undef LOOPLIST
				#define LOOPLIST savecarmods
				#undef INDEXAMOUNT
				#define INDEXAMOUNT garagesamount
				INDEXFUNCTION LOOPFUNCTION

				#undef FINDMAP
				#define FINDMAP clothesauid3s

				#define CLEARLIST CLOTHESMODELS
				#define CLEARENTITIES saveclothmodels
				#define CLEARENTITY SaveClothModel
				#define CLOTHESCLEAR \
					{ \
						auto clothpart = int(0); \
						for (auto &auid3id : CLEARLIST) { \
							FINDAUID3 \
								auto saveentity = CLEARENTITY( \
										0 \
									,	auid3find->second \
									,	clothpart \
								); \
								CLEARENTITIES.push_back(saveentity); \
								auid3id = 0; \
							} \
							++clothpart; \
						} \
					}
				CLOTHESCLEAR

				#undef CLEARLIST
				#define CLEARLIST CLOTHESTEXTURES
				#undef CLEARENTITIES
				#define CLEARENTITIES saveclothtextures
				#undef CLEARENTITY
				#define CLEARENTITY SaveClothTexture
				CLOTHESCLEAR

				#undef FINDMAP
				#define FINDMAP blipsauid3s

				#undef LOOPAMOUNT
				#define LOOPAMOUNT blipsamount
				#undef LOOPPOINTER
				#define LOOPPOINTER blipsSprite
				#undef LOOPGET
				#define LOOPGET GETBLIPSSPRITE
				#undef LOOPCALL
				#define LOOPCALL LOOPID
				#undef LOOPCLASS
				#define LOOPCLASS SaveBlipSprite
				#undef LOOPEXTEND
				#define LOOPEXTEND 
				#undef LOOPSET
				#define LOOPSET SETBLIPSSPRITE
				#undef LOOPCLEAR
				#define LOOPCLEAR eRadarSprite::RADAR_SPRITE_MAP_HERE
				#undef LOOPLIST
				#define LOOPLIST saveblipsprites
				LOOPFUNCTION

				#undef FINDMAP
				#define FINDMAP combosauid3s

				auto &auid3id = PLAYERCOMBO;
				FINDAUID3
					auid3id = eFightingStyle::STYLE_STANDARD;
					saveplayercombo.id = auid3find->first;
					saveplayercombo.name = auid3find->second;
				}
				//<limiter
			};

			CdeclEvent <AddressList< //CGenericGameStorage::GenericSave
					0x619081, H_CALL //C_PcSave::SaveSlot
			>, PRIORITY_AFTER, ArgPickNone, void()> CGenericGameStorage__GenericSaveEvent;
			CGenericGameStorage__GenericSaveEvent += [] {
				savePaths(CGenericGameStorage::ms_SaveFileNameJustSaved);

				//limiter>
				#define SAVEENTITIES saveplayerweapons
				#define SAVEINI saveiniplayerweapons
				#define SAVEWRITE \
					if (SAVEENTITIES.size() > 0) { \
						fs::create_directories(fs::path(SAVEINI.GetIniPath()).parent_path()); \
						auto iniindex = int(1); \
						for (auto saveentity : SAVEENTITIES) { \
							auto inistring = to_string(iniindex); \
							SAVEINI.WriteString(inistring, "name", saveentity.name);
				#define SAVEWRITE2 \
							++iniindex; \
						} \
						SAVEINI.WriteInteger("total", "total", SAVEENTITIES.size()); \
					}
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.sammo, saveentity.ammo);
					SAVEINI.WriteBoolean(inistring, saveentity.scurrent, saveentity.current);
					SAVEINI.WriteInteger(inistring, saveentity.sclip, saveentity.clip);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES savegangweapons
				#undef SAVEINI
				#define SAVEINI saveinigangweapons
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.sgang, saveentity.gang);
					SAVEINI.WriteInteger(inistring, saveentity.sweapon, saveentity.weapon);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES savepickupmodels
				#undef SAVEINI
				#define SAVEINI saveinipickupmodels
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.spickup, saveentity.pickup);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES savegaragecars
				#undef SAVEINI
				#define SAVEINI saveinigaragecars
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.sgarage, saveentity.garage);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES savegeneratorcars
				#undef SAVEINI
				#define SAVEINI saveinigeneratorcars
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.sgenerator, saveentity.generator);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES savecarmods
				#undef SAVEINI
				#define SAVEINI saveinicarmods
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.smod, saveentity.mod);
					SAVEINI.WriteInteger(inistring, saveentity.sgarage, saveentity.garage);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES saveclothmodels
				#undef SAVEINI
				#define SAVEINI saveiniclothmodels
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.spart, saveentity.part);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES saveclothtextures
				#undef SAVEINI
				#define SAVEINI saveiniclothtextures
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.spart, saveentity.part);
				SAVEWRITE2

				#undef SAVEENTITIES
				#define SAVEENTITIES saveblipsprites
				#undef SAVEINI
				#define SAVEINI saveiniblipsprites
				SAVEWRITE
					SAVEINI.WriteInteger(inistring, saveentity.sblip, saveentity.blip);
				SAVEWRITE2

				if (saveplayercombo.id > 0) {
					saveiniplayercombo.WriteString(saveplayercombo.scombo, saveplayercombo.scombo, saveplayercombo.name);
				}
				//<limiter

				saveApply();
			};

			CdeclEvent <AddressList< //C3dMarkers::LoadUser3dMarkers
					0x5D19CE, H_CALL //GenericLoad
			>, PRIORITY_AFTER, ArgPickNone, void()> C3dMarkers__LoadUser3dMarkersEvent;
			C3dMarkers__LoadUser3dMarkersEvent += [] {
				savePaths(CGenericGameStorage::ms_LoadFileNameWithPath);

				auto initotal = int();

				//limiter>
				#define LOADINI saveiniplayerweapons
				#define LOADENTITIES saveplayerweapons
				#define LOADENTITY SavePlayerWeapon
				#define SAVEREAD \
					initotal = LOADINI.ReadInteger("total", "total", 0); \
					if (initotal > 0) { \
						for (auto iniindex = int(1); iniindex <= initotal; ++iniindex) { \
							auto inistring = to_string(iniindex); \
							auto auid3name = LOADINI.ReadString(inistring, "name", ""); \
							auto auid3find = auid3sids.find(auid3name); \
							if (auid3find != auid3sids.end()) { \
								auto e = LOADENTITY(); \
								auto saveentity = LOADENTITY( \
										auid3find->second \
									,	auid3name
				#define SAVEREAD2 \
								); \
								delete(&e); \
								LOADENTITIES.push_back(saveentity); \
							} \
						} \
					}
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.sammo, 0)
					, LOADINI.ReadBoolean(inistring, e.scurrent, false)
					, LOADINI.ReadInteger(inistring, e.sclip, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveinigangweapons
				#undef LOADENTITIES
				#define LOADENTITIES savegangweapons
				#undef LOADENTITY
				#define LOADENTITY SaveGangWeapon
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.sgang, 0)
					, LOADINI.ReadInteger(inistring, e.sweapon, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveinipickupmodels
				#undef LOADENTITIES
				#define LOADENTITIES savepickupmodels
				#undef LOADENTITY
				#define LOADENTITY SavePickupModel
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.spickup, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveinigaragecars
				#undef LOADENTITIES
				#define LOADENTITIES savegaragecars
				#undef LOADENTITY
				#define LOADENTITY SaveGarageCar
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.sgarage, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveinigeneratorcars
				#undef LOADENTITIES
				#define LOADENTITIES savegeneratorcars
				#undef LOADENTITY
				#define LOADENTITY SaveGeneratorCar
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.sgenerator, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveinicarmods
				#undef LOADENTITIES
				#define LOADENTITIES savecarmods
				#undef LOADENTITY
				#define LOADENTITY SaveCarMod
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.smod, 0)
					, LOADINI.ReadInteger(inistring, e.sgarage, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveiniclothmodels
				#undef LOADENTITIES
				#define LOADENTITIES saveclothmodels
				#undef LOADENTITY
				#define LOADENTITY SaveClothModel
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.spart, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveiniclothtextures
				#undef LOADENTITIES
				#define LOADENTITIES saveclothtextures
				#undef LOADENTITY
				#define LOADENTITY SaveClothTexture
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.spart, 0)
				SAVEREAD2

				#undef LOADINI
				#define LOADINI saveiniblipsprites
				#undef LOADENTITIES
				#define LOADENTITIES saveblipsprites
				#undef LOADENTITY
				#define LOADENTITY SaveBlipSprite
				SAVEREAD
					, LOADINI.ReadInteger(inistring, e.sblip, 0)
				SAVEREAD2

				{
					saveplayercombo.name = saveiniplayercombo.ReadString(saveplayercombo.scombo, saveplayercombo.scombo, "");
					auto auid3find = auid3sids.find(saveplayercombo.name);
					if (auid3find != auid3sids.end()) {
						saveplayercombo.id = auid3find->second;
					}
				}
				//<limiter

				saveApply();
			};

			//limiter>
			#define BOXBOOL messagemodels
			#define BOXTYPE MESSAGEMODEL
			#define BOXMESSAGE if (BOXBOOL) modMessage(MESSAGEFREE + BOXTYPE + MESSAGEIDS ASSIGNSTOP, MB_ICONERROR);
			BOXMESSAGE

			#undef BOXBOOL
			#define BOXBOOL messageweapons
			#undef BOXTYPE
			#define BOXTYPE MESSAGEWEAPON
			BOXMESSAGE

			#undef BOXBOOL
			#define BOXBOOL messageblips
			#undef BOXTYPE
			#define BOXTYPE MESSAGEBLIP
			BOXMESSAGE

			#undef BOXBOOL
			#define BOXBOOL messagecombos
			#undef BOXTYPE
			#define BOXTYPE MESSAGECOMBO
			BOXMESSAGE

			#undef BOXBOOL
			#define BOXBOOL messagetrains
			#undef BOXTYPE
			#define BOXTYPE MESSAGETRAIN
			BOXMESSAGE
			//<limiter
			if (messageunassigned) modMessage("One or more AUID3s have unassigned"  + MESSAGEIDS ASSIGNSTOP, MB_ICONERROR);

			logmain.writeText("Approximate time taken: " + to_string((chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - timerstart)).count()) + "ms.");
		}
		else {
			modMessage(modname + " will not work for this session.", MB_ICONWARNING);
		}
	}
	else {
		logmain.writeText("No AUID3 files found.");
	}
//<
    }
//>
//<
} autoID3000;

//>
//<
