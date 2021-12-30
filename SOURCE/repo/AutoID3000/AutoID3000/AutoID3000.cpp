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
//<

using namespace plugin;
//>
using namespace std;

namespace fs = filesystem;

auto timerstart = chrono::high_resolution_clock::now();

#define EXPORTFUNCTION extern "C" __declspec(dllexport)

auto modversion = int(3000);
EXPORTFUNCTION auto modVersion() {
	return modversion;
}
auto modname = string("AutoID3000");
auto modMessage(string messagetext, int messageflags = MB_OK) {
	return MessageBox(NULL, messagetext.c_str(), modname.c_str(), messageflags);
}

auto folderroot = fs::path(GAME_PATH(""));
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
auto logunassigned = LogFile("unassigned");

class LoadedModule {
public:
	fs::path modulepath;
	bool isinstalled = false;
	LoadedModule(fs::path modulestem, fs::path mlfolder) {
		auto modulename = modulestem.replace_extension(".asi");
		if (fs::exists(folderroot / modulename)) {
			modulepath = folderroot;
			isinstalled = true;
		}
		else if (fs::exists(folderscripts / modulename)) {
			modulepath = folderscripts;
			isinstalled = true;
		}
		else if (fs::exists(folderml / mlfolder / modulename)) {
			modulepath = folderml / mlfolder;
			isinstalled = true;
		}
	}
};
auto loadedml = LoadedModule("modloader", "");
auto loadedfla = LoadedModule("$fastman92limitAdjuster", "fastman92 Limit Adjuster");
auto loadedola = LoadedModule("III.VC.SA.LimitAdjuster.", "Open Limit Adjuster");

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

auto copyWait(fs::path sourcepath, fs::path targetpath) {
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
		Sleep(10);
	}
	temporaryfile.close();
	fs::remove(targetpath);
	fs::rename(temporarypath, targetpath);
	fs::remove(temporarypath);
}

auto outputroot = (casePath(folderroot) / fs::path()).string();
auto outputlength = outputroot.length();
auto OutputPath(fs::path filepath) {
	filepath = casePath(filepath);
	auto pathstring = filepath.string();
	while (true) {
		auto pathfind = pathstring.find(outputroot);
		if (pathfind == string::npos) break;
		pathstring.erase(pathfind, outputlength);
	}
	return pathstring;
}

auto storagebackups = fs::path();

class BackupFile {
public:
	fs::path originalpath;
	fs::path backuppath;
	bool originalexisted = false;
	BackupFile(fs::path originallocation) {
		originalpath = casePath(originallocation);
		backuppath = folderbackups / originalpath.filename();
		if (fs::exists(originalpath)) {
			originalexisted = true;
		}
	}
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
			copyWait(backuppath, originalpath);
			logbackups.writeText("Restored backup of file \"" + OutputPath(originalpath) + "\" at \"" + OutputPath(backuppath) + "\".");
			deleteBackup();
		}
	}
	auto createBackup() {
		if (backupExists()) {
			if (modMessage("File \"" + OutputPath(originalpath) + "\" already found backed-up at \"" + OutputPath(backuppath) + "\". Restore it before deleting?", MB_YESNO | MB_ICONWARNING) == IDYES) {
				originalexisted = true;
				restoreBackup();
			}
		}
		deleteBackup();
		if (originalexisted) {
			copyWait(originalpath, storagebackups / backuppath.filename());

			copyWait(originalpath, backuppath);
			logbackups.writeText("Created backup of file \"" + OutputPath(originalpath) + "\" at \"" + OutputPath(backuppath) + "\".");
		}
		else {
			if (modMessage("File \"" + OutputPath(originalpath) + "\" not found when it was needed. Would you like for the game to automatically close after loading? The file will probably have been generated by then.", MB_YESNO | MB_ICONERROR) == IDYES) {
				Events::processScriptsEvent += [] {
					modMessage("The game will now automatically close.", MB_ICONWARNING);
					exit(EXIT_SUCCESS);
				};
			}
		}
	}
};
auto backupflaconfig = BackupFile(loadedfla.modulepath / fs::path("fastman92limitAdjuster_GTASA").replace_extension(".ini"));
auto backupflaweapon = BackupFile(loadedfla.modulepath / fs::path("data") / fs::path("gtasa_weapon_config").replace_extension(".dat"));
auto backupflaaudio = BackupFile(loadedfla.modulepath / fs::path("data") / fs::path("gtasa_vehicleAudioSettings").replace_extension(".cfg"));
auto backupflamodel = BackupFile(loadedfla.modulepath / fs::path("data") / fs::path("model_special_features").replace_extension(".dat"));

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
auto filesml = vector<fs::path>();
auto filesauid3 = filesml;
auto fileside = filesml;
auto filestxt = filesml;

auto deleteGenerated() {
	for (auto filepath : filesauid3) {
		fs::remove(filepath.replace_extension(".ide"));
		fs::remove(filepath.replace_extension(".ipl"));
		fs::remove(filepath.replace_extension(".txt"));
	}
}

auto readDat(fs::path datstem) {
	auto datfile = fstream();
	datfile.open(folderroot / fs::path("data") / datstem.replace_extension(".dat"));
	if (datfile.is_open()) {
		auto datline = string();
		while (getline(datfile, datline)) {
			if (
				datline.length() > 4
				&& lowerString(datline).substr(0, 4) == "ide "
				) {
				fileside.push_back(folderroot / fs::path(datline.substr(4)));
			}
		}
		datfile.close();
	}
}

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
			logfiles.writeText(OutputPath(filepath));
		}
		filehandle.close();
	}
}
auto linesfiles = map<fs::path, vector<string>>();
auto linesAdd(fs::path linessection, string sectionline) {
	if (
		linessection == "fla:"
		|| linessection == "flb:"
		|| linessection == "flc:"
		) {
		if (
			sectionline.length() > 0
			&& sectionline[0] == '@'
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
		Gflafile << endl;
		AUID3Iterator(flasection, skipemptylines, [](string flaline, string flalower) {
			Gflafile << flaline << endl;
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
	AUID3Iterator("flc:", true, [](string flaline, string flalower) {
		if (FLAScan(flalower, &flasection, &flakey, true, &fladefault, &flavalue)) {
			delete(flaini); flaini = new CIniReader(backupflaconfig.originalpath.string());
			if (flavalue > flaini->ReadInteger(flasection, flakey, fladefault)) flaini->WriteInteger(flasection, flakey, flavalue);
		}
	});
	AUID3Iterator("fla:", true, [](string flaline, string flalower) {
		if (FLAScan(flalower, &flasection, &flakey, true, &fladefault, &flavalue)) {
			delete(flaini); flaini = new CIniReader(backupflaconfig.originalpath.string());
			flaini->WriteInteger(flasection, flakey, flaini->ReadInteger(flasection, flakey, fladefault) + flavalue);
		}
	});
	AUID3Iterator("flb:", true, [](string flaline, string flalower) {
		if (FLAScan(flalower, &flasection, &flakey, false)) {
			delete(flaini); flaini = new CIniReader(backupflaconfig.originalpath.string());
			flaini->WriteInteger(flasection, flakey, 1);
		}
	});
	delete(flaini);
}

auto limitmodels = int(20000);
auto limitkillables = int(800);
auto limitweapons = int(70);

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

auto weaponsused = modelsused;
auto weaponScan(string weaponline) {
	auto weaponseparator = weaponline.find_first_of(' ');
	if (weaponseparator != string::npos) {
		auto weaponstring = weaponline.substr(0, weaponseparator);
		auto weaponid = int();
		if (sscanf(weaponstring.c_str(), "%d", &weaponid) == 1) {
			weaponsused.insert(weaponid);
		}
	}
}

auto freemodels = modelsused;
auto freekillables = modelsused;
auto freeweapons = modelsused;

auto auid3sids = map<fs::path, int>();

class SavePlayerWeapon {
public:
	SavePlayerWeapon() {}
	int id;
	string name;
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
class SaveGangWeapon {
public:
	SaveGangWeapon() {}
	int id;
	string name;
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
class SavePickupModel {
public:
	SavePickupModel() {}
	int id;
	string name;
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
class SaveGarageCar {
public:
	SaveGarageCar() {}
	int id;
	string name;
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
class SaveGeneratorCar {
public:
	SaveGeneratorCar() {}
	int id;
	string name;
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
class SaveCarMod {
public:
	SaveCarMod() {}
	int id;
	string name;
	int garage; string sgarage = "garage";
	int mod; string smod = "mod";
	SaveCarMod(
			int modid
		,	string modname
		,	int modgarage
		,	int modmod
	) {
		id = modid;
		name = modname;
		garage = modgarage;
		mod = modmod;
	}

};
class SaveClothModel {
public:
	SaveClothModel() {}
	string name;
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
class SaveClothTexture {
public:
	SaveClothTexture() {}
	string name;
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
auto saveplayerweapons = vector<SavePlayerWeapon>();
auto savegangweapons = vector<SaveGangWeapon>();
auto savepickupmodels = vector<SavePickupModel>();
auto savegaragecars = vector<SaveGarageCar>();
auto savegeneratorcars = vector<SaveGeneratorCar>();
auto savecarmods = vector<SaveCarMod>();
auto saveclothmodels = vector<SaveClothModel>();
auto saveclothtextures = vector<SaveClothTexture>();
auto saveini = fs::path();
auto saveINI(CIniReader *inifile, fs::path inistem) {
	inifile->~CIniReader();
	new(inifile) CIniReader((saveini / inistem.replace_extension(".ini")).string());
}
auto saveiniplayerweapons = CIniReader();
auto saveinigangweapons = saveiniplayerweapons;
auto saveinipickupmodels = saveiniplayerweapons;
auto saveinigaragecars = saveiniplayerweapons;
auto saveinigeneratorcars = saveiniplayerweapons;
auto saveinicarmods = saveiniplayerweapons;
auto saveiniclothmodels = saveiniplayerweapons;
auto saveiniclothtextures = saveiniplayerweapons;
auto savePaths(fs::path savelocation) {
	auto savepath = fs::path(savelocation);
	auto savehash = int(0);
	auto savefile = fstream();
	savefile.open(savepath, fstream::in | fstream::binary);
	if (savefile.is_open()) {
		auto savesize = fs::file_size(savepath);
		auto savebuffer = vector<char>(savesize);
		if (savefile.read(savebuffer.data(), savesize)) {
			savehash = CRC::Calculate(savebuffer.data(), savesize, CRC::CRC_32());
		}
		savefile.close();
	}
	saveini = savepath.parent_path() / fs::path(modname) / fs::path(to_string(savehash));
	saveINI(&saveiniplayerweapons, "playerweapons");
	saveINI(&saveinigangweapons, "gangweapons");
	saveINI(&saveinipickupmodels, "pickupmodels");
	saveINI(&saveinigaragecars, "garagecars");
	saveINI(&saveinigeneratorcars, "generatorcars");
	saveINI(&saveinicarmods, "carmods");
	saveINI(&saveiniclothmodels, "clothmodels");
	saveINI(&saveiniclothtextures, "clothtextures");
}

auto pointersread = bool(false);
auto playerpointer = (CPlayerPed *)nullptr;
auto idsauid3s = map<int, string>();
auto weaponsauid3s = idsauid3s;
auto clothesauid3s = idsauid3s;

#define POINTERSSTART unsigned int
#define POINTERSSIZE unsigned char

auto gangsstart = POINTERSSTART();
auto pickupsstart = POINTERSSTART();
auto garagesstart = POINTERSSTART();
auto generatorsstart = POINTERSSTART();

auto gangssize = POINTERSSIZE();
auto pickupssize = POINTERSSIZE();
auto garagessize = POINTERSSIZE();
auto generatorssize = POINTERSSIZE();
auto modssize = POINTERSSIZE();

auto gangsweapon = POINTERSSIZE();
auto pickupsmodel = POINTERSSIZE();
auto garagesmodel = POINTERSSIZE();
auto generatorsmodel = POINTERSSIZE();
auto modsmodel = POINTERSSIZE();

auto gangsamount = POINTERSSTART();
auto pickupsamount = POINTERSSTART();
auto garagesamount = POINTERSSIZE();
auto generatorsamount = POINTERSSTART();
auto modsamount = POINTERSSTART();

#define SETGANGSWEAPON SetUInt
#define SETPICKUPSMODEL SetUShort
#define SETGARAGESMODEL SETPICKUPSMODEL
#define SETGENERATORSMODEL SETPICKUPSMODEL
#define SETMODSMODEL SETPICKUPSMODEL

#define GETGANGSWEAPON GetUInt
#define GETPICKUPSMODEL GetUShort
#define GETGARAGESMODEL GETPICKUPSMODEL
#define GETGENERATORSMODEL GETPICKUPSMODEL
#define GETMODSMODEL GETPICKUPSMODEL

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
auto modsPointer(int garageid, int modid) {
	return (garagesPointer(garageid) + (modid * modssize));
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
auto modsModel(int garageid, int modid) {
	return POINTERSSTART(modsPointer(garageid, modid)) + modsmodel;
}

#define CLOTHESMODELS playerpointer->m_pPlayerData->m_pPedClothesDesc->m_anModelKeys
#define CLOTHESTEXTURES playerpointer->m_pPlayerData->m_pPedClothesDesc->m_anTextureKeys
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

		for (auto auid3data : auid3sids) {
			auto auid3string = auid3data.first.string();
			auto auid3id = auid3data.second;
			if (auid3string[0] == '&') weaponsauid3s[auid3id] = auid3string;
			else if (auid3string[0] == '@') clothesauid3s[clothesHash(auid3string)] = auid3string;
			else idsauid3s[auid3id] = auid3string;
		}

		gangsstart = patch::GetUInt(0x5D3A93);
		pickupsstart = patch::GetUInt(0x48ADC3);
		garagesstart = patch::GetUInt(0x443A88) + 0x4;
		generatorsstart = patch::GetUInt(0x6F3F88);

		gangssize = patch::GetUChar(0x5D3AA1);
		pickupssize = patch::GetUChar(0x4590E1);
		garagessize = patch::GetUChar(0x5D3309);
		generatorssize = patch::GetUChar(0x6F32AA);
		modssize = patch::GetUChar(0x447E8D);

		gangsweapon = 0x4;
		pickupsmodel = patch::GetUChar(0x5D35B4);
		garagesmodel = patch::GetUChar(0x447E5F);
		generatorsmodel = 0;
		modsmodel = patch::GetUChar(0x447E6D);

		gangsamount = patch::GetUInt(0x5D3A98);
		pickupsamount = patch::GetUInt(0x457190);
		garagesamount = patch::GetUChar(0x5D3345);
		generatorsamount = patch::GetUInt(0x6F3F7D);
		modsamount = patch::GetUInt(0x447E6F);

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

	auto weaponactive = playerpointer->m_aWeapons[playerpointer->m_nActiveWeaponSlot].m_nType;
	auto weaponclips = vector<pair<int, int>>();
	SAVEAPPLY
		playerpointer->MakeChangesForNewWeapon(saveentity.id);
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
			patch::SETMODSMODEL(modsModel(saveentity.garage, saveentity.mod), saveentity.id, false);
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
}
//<

class AutoID3000 {
public:
    AutoID3000() {
        // Initialise your plugin here
        
//>
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
				echo    1. Backups exist.
			goto exitScript

			:isEmpty
				echo Uninstall freely:
				echo    1. Go to the game directory.
				echo    2. Delete the file "#AutoID3000.asi".
				echo    3. Optional: delete the folder "AutoID3000".
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

		if (loadedml.isinstalled) {
			logmain.writeText("Mod Loader installed.");
		}
		else {
			logmain.writeText("Mod Loader not installed.");
		}

		if (loadedfla.isinstalled) {
			logmain.writeText("fastman92 Limit Adjuster installed.");

			backupflaconfig.createBackup();
			backupflaweapon.createBackup();
			Events::initPoolsEvent.before += [] {backupflaaudio.createBackup(); };
			Events::initPoolsEvent.before += [] {backupflamodel.createBackup(); };

			Events::attachRwPluginsEvent += [] {backupflaconfig.restoreBackup(); };
			Events::attachRwPluginsEvent += [] {backupflaweapon.restoreBackup(); };
			Events::initGameEvent.after += [] {backupflaaudio.restoreBackup(); };
			Events::initGameEvent.after += [] {backupflamodel.restoreBackup(); };
		}
		else {
			logmain.writeText("fastman92 Limit Adjuster not installed.");
		}

		if (loadedola.isinstalled) {
			logmain.writeText("Open Limit Adjuster installed.");
		}
		else {
			logmain.writeText("Open Limit Adjuster not installed.");
		}

		logmain.newLine();

		if (
			(!loadedfla.isinstalled
				|| (
					backupflaconfig.originalexisted
					&& backupflaweapon.originalexisted
					&& backupflaaudio.originalexisted
					&& backupflamodel.originalexisted
					)
				)
			//&& 
			) {
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
					if (fileextension == ".auid3") filesauid3.push_back(filepath);
				}
			});

			readDat("default");
			readDat("gta");

			if (loadedml.isinstalled) {
				filesAll(folderml, [](fs::path entrypath, fs::recursive_directory_iterator searchhandle) {
					if (entrypath.stem().string()[0] == '.') {
						if (fs::is_directory(entrypath)) {
							searchhandle.disable_recursion_pending();
						}
					}
					else {
						filesml.push_back(entrypath);
					}
				});

				{
					//filter 'filesml' by replicating Mod Loader behaviour
					//ignore files and folders beginning with a '.' (dot): DONE

				}

				for (auto filepath : filesml) {
					auto fileextension = lowerString(filepath.extension().string());
					if (fileextension == ".auid3") filesauid3.push_back(filepath);
					else if (fileextension == ".ide") fileside.push_back(filepath);
					else if (fileextension == ".txt") filestxt.push_back(filepath);
				}
			}

			deleteGenerated();
			Events::shutdownRwEvent += deleteGenerated;

			static auto Gfilesection = bool(false);
			static auto Gfileskip = bool(false);
			static auto Glinenumber = int(0);

			static auto Gauid3current = fs::path();
			static auto Gauid3path = fs::path();
			static auto Gauid3fla = bool(false);
			logfiles.writeText("Read AUID3 files:");
			for (auto filepath : filesauid3) {
				auid3names.insert(filepath.stem().string());

				Gfilesection = false;
				Gfileskip = false;
				Glinenumber = 0;

				Gauid3current = fs::path();
				Gauid3path = filepath;
				Gauid3fla = false;
				linesFile(filepath, true, [](string fileline, string linelower) {
					++Glinenumber;
					if (linelower == "end:") Gfilesection = false;
					else if (linelower == "def:") Gfilesection = true, Gfileskip = true, Gauid3current = "def:", Gauid3fla = false;
					else if (linelower == "ide:") Gfilesection = true, Gfileskip = true, Gauid3current = Gauid3path, Gauid3current.replace_extension(".ide"), Gauid3fla = false;
					else if (linelower == "ipl:") Gfilesection = true, Gfileskip = true, Gauid3current = Gauid3path, Gauid3current.replace_extension(".ipl"), Gauid3fla = false;
					else if (linelower == "txt:") Gfilesection = true, Gfileskip = true, Gauid3current = Gauid3path, Gauid3current.replace_extension(".txt"), Gauid3fla = false;
					else if (
						linelower.length() > 5
						&& linelower.substr(0, 5) == "fil: "
						) {
						Gfilesection = true;
						Gfileskip = true;
						Gauid3current = Gauid3path.parent_path() / fileline.substr(5);
						Gauid3fla = false;
						auto fileextension = lowerString(Gauid3current.extension().string());
						if (
							fileextension == ".ide"
							|| fileextension == ".ipl"
							|| fileextension == ".txt"
							) {
							Gfilesection = false;
							logignored.writeText("Ignored " + modname + " section \"" + fileline + "\" in file \"" + OutputPath(Gauid3path) + "\" at line " + to_string(Glinenumber) + " because of unsupported extension.");
						}
					}
					else if (linelower == "fla:") Gfilesection = true, Gfileskip = true, Gauid3current = "fla:", Gauid3fla = true;
					else if (linelower == "flb:") Gfilesection = true, Gfileskip = true, Gauid3current = "flb:", Gauid3fla = true;
					else if (linelower == "flc:") Gfilesection = true, Gfileskip = true, Gauid3current = "flc:", Gauid3fla = true;
					else if (linelower == "wpn:") Gfilesection = true, Gfileskip = true, Gauid3current = "wpn:", Gauid3fla = true;
					else if (linelower == "fls:") Gfilesection = true, Gfileskip = true, Gauid3current = "fls:", Gauid3fla = true;
					else if (linelower == "spc:") Gfilesection = true, Gfileskip = true, Gauid3current = "spc:", Gauid3fla = true;

					if (
						Gfilesection
						&& !Gfileskip
						) {
						if (lowerString(Gauid3current.extension().string()) == ".ide") lineside.push_back(fileline);

						auto auid3string = Gauid3current.string();
						if (auid3string == "wpn:") weaponScan(fileline);

						linesAdd(Gauid3current, fileline);
					}
					Gfileskip = false;
				});
			}
			logfiles.newLine();

			if (
				!loadedfla.isinstalled
				&& Gauid3fla
				) {
				if (modMessage("Current " + modname + " configuration requires fastman92 Limit Adjuster which could not be detected. Would you like to close the game? It will probably crash anyway.", MB_YESNO | MB_ICONWARNING) == IDYES) {
					exit(EXIT_SUCCESS);
				}
			}

			AUID3Iterator("def:", true, [](string auid3line, string auid3lower) {
				auid3names.insert(auid3line);
			});

			auto arekillable = int(0);
			auto areped = arekillable;
			auto arevehicle = arekillable;
			auto arebike = arekillable;
			auto areflying = arekillable;
			auto arewater = arekillable;
			auto areweapon = arekillable;
			auto arecloth = arekillable;
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
					firstchar != '$'
					&& secondchar != '$'
					) {
					auto isped = firstchar == '_';
					auto isvehicle = firstchar == '^';
					if (isped || isvehicle) {
						++arekillable;
						if (isped) {
							++areped;
						}
						else if (isvehicle) {
							++arevehicle;
							if (secondchar == 'b') {
								++arebike;
							}
							else if (secondchar == 'f') {
								++areflying;
							}
							else if (secondchar == 'w') {
								++arewater;
							}
						}
					}
					else if (firstchar == '&') {
						++areweapon;
					}
					//else if (firstchar == '%') {
					//}
					//else if (firstchar == ',') {
					//}
					else if (firstchar == '@') {
						++arecloth;
					}
					else {
						if (firstchar == '`') {
							++aredamage;
						}
						else if (firstchar == '+') {
							++aretimed;
						}
						else if (firstchar == '-') {
							++arehier;
						}
						else {
							++areobject;
						}
						if (secondchar == '.') {
							++areinfo;
						}
						if (thirdchar != '&') {
							++arecol;
						}
					}
				}
			}
			if (arekillable > 0) {
				linesAdd("flc:", "id limits:count of killable model ids:800:10000");
			}
			if (areped > 0) {
				linesAdd("fla:", "@ide limits:ped models:278:" + to_string(areped));
				linesAdd("flc:", "ped streaming:pedgrp peds per group:21:210");
			}
			if (arevehicle > 0) {
				linesAdd("fla:", "@dynamic limits:vehiclestructs:50:2500");
				linesAdd("fla:", "@ide limits:vehicle models:212:" + to_string(arevehicle));
				linesAdd("flc:", "car streaming:cargrp cars per group:23:63");
				linesAdd("flb:", "car streaming:accept any id for car generator");
				linesAdd("flb:", "handling.cfg limits:apply handling.cfg patch");
				linesAdd("fla:", "handling.cfg limits:number of standard lines:210:" + to_string(arevehicle));
				linesAdd("flc:", "other limits:vehicle colors:128:255");
				linesAdd("flb:", "special:make paintjobs work for any id");
				linesAdd("flb:", "addons:enable vehicle audio loader");
			}
			if (arebike > 0) {
				linesAdd("fla:", "handling.cfg limits:number of bike lines:13:" + to_string(arebike));
			}
			if (areflying > 0) {
				linesAdd("fla:", "handling.cfg limits:number of flying lines:24:" + to_string(areflying));
			}
			if (arewater > 0) {
				linesAdd("fla:", "handling.cfg limits:number of boat lines:12:" + to_string(arewater));
			}
			if (areweapon > 0) {
				linesAdd("fla:", "@ide limits:weapon models:51:" + to_string(areweapon));
				linesAdd("flb:", "weapon limits:enable weapon type loader");
				linesAdd("fla:", "weapon limits:weapon type loader, number of type ids:70:" + to_string(areweapon));
			}
			if (arecloth > 0) {
				linesAdd("fla:", "directory limits:clothes directory:550:" + to_string(arecloth));
			}
			if (aredamage > 0) {
				linesAdd("fla:", "@ide limits:ide objects type 2:70:" + to_string(aredamage));
			}
			if (aretimed > 0) {
				linesAdd("fla:", "@ide limits:timed objects:169:" + to_string(aretimed));
			}
			if (arehier > 0) {
				linesAdd("fla:", "@ide limits:hier objects:92:" + to_string(arehier));
			}
			if (areobject > 0) {
				linesAdd("fla:", "@ide limits:ide objects type 1:14000:" + to_string(areobject));
			}
			if (areinfo > 0) {
				linesAdd("fla:", "other limits:object info entries:160:" + to_string(areinfo));
			}
			if (arecol > 0) {
				linesAdd("fla:", "@dynamic limits:colmodels:10150:" + to_string(arecol));
			}

			for (auto modelid = int(0); modelid <= 10; ++modelid) modelsused.insert(modelid);
			for (auto modelid = int(374); modelid <= 383; ++modelid) modelsused.insert(modelid);
			for (auto modelid = int(15000); modelid <= 15024; ++modelid) modelsused.insert(modelid);
			for (auto weaponid = int(0); weaponid <= 69; ++weaponid) weaponsused.insert(weaponid);

			logfiles.writeText("Read IDE files:");
			for (auto filepath : fileside) {
				linesFile(filepath, true, [](string fileline, string linelower) {
					lineside.push_back(fileline);
				});
			}
			logfiles.newLine();

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

			logfiles.writeText("Read TXT files:");
			for (auto filepath : filestxt) {
				linesFile(filepath, true, [](string fileline, string linelower) {
					modelScan(fileline);
				});
			}
			logfiles.newLine();

			if (loadedfla.isinstalled) {
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

				auto flalimits = CIniReader(backupflaconfig.originalpath.string());
				if (flalimits.ReadInteger("id limits", "apply id limit patch", 1) == 1) {
					limitmodels = flalimits.ReadInteger("id limits", "file_type_dff", limitmodels);
				}

				limitkillables = flalimits.ReadInteger("id limits", "count of killable model ids", limitkillables);

				if (flalimits.ReadInteger("weapon limits", "enable weapon type loader", 1) == 1) {
					limitweapons = flalimits.ReadInteger("weapon limits", "weapon type loader, number of type ids", limitweapons);
				}
			}

			limitmodels--;
			limitkillables--;
			limitweapons--;

			logmain.writeText("Highest killable model ID: " + to_string(limitkillables));
			logmain.writeText("Highest regular model ID: " + to_string(limitmodels));
			logmain.writeText("Highest weapon ID: " + to_string(limitweapons));

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
				linesFile(backupflaweapon.originalpath, false, [](string fileline, string linelower) {
					weaponScan(fileline);
				});
			}

			for (auto weaponid = int(0); weaponid <= limitweapons; ++weaponid) {
				if (weaponsused.find(weaponid) == weaponsused.end()) {
					freeweapons.insert(weaponid);
				}
			}

			auto indexmodels = int(0);
			auto indexkillables = indexmodels;
			auto indexweapons = indexmodels;

			auto maxmodels = freemodels.size();
			auto maxkillables = freekillables.size();
			auto maxweapons = freeweapons.size();

			auto messagemodels = bool(false);
			auto messageweapons = bool(false);

			for (auto auid3name : auid3names) {
				auto firstchar = char(); if (auid3name.length() > 0) firstchar = auid3name[0];
				if (firstchar != '$') {
					auto isped = firstchar == '_';
					auto isvehicle = firstchar == '^';
					if (isped || isvehicle) {
						if (indexkillables < maxkillables) {
							auto assignedid = *next(freekillables.begin(), indexkillables);
							auid3sids[auid3name] = assignedid;
							++indexkillables;
							logassigned.writeText("Assigned killable model ID " + to_string(assignedid) + " to " + auid3name + ".");
						}
						else if (indexmodels < maxmodels) {
							auto assignedid = *next(freemodels.begin(), indexmodels);
							auid3sids[auid3name] = assignedid;
							++indexmodels;
							logassigned.writeText("Assigned regular model ID " + to_string(assignedid) + " to " + auid3name + " (forced).");
						}
						else {
							messagemodels = true;
							logassigned.writeText("Run out of free model IDs to assign any to " + auid3name + ".");
						}
					}
					else if (firstchar == '&') {
						if (indexweapons < maxweapons) {
							auto assignedid = *next(freeweapons.begin(), indexweapons);
							auid3sids[auid3name] = assignedid;
							++indexweapons;
							logassigned.writeText("Assigned weapon ID " + to_string(assignedid) + " to " + auid3name + ".");
						}
						else {
							messageweapons = true;
							logassigned.writeText("Run out of free weapon IDs to assign any to " + auid3name + ".");
						}
					}
					//else if (firstchar == '%') {
					//}
					//else if (firstchar == ',') {
					//}
					else if (firstchar == '@') {
						auid3sids[auid3name] = 0;
						logassigned.writeText("Assigned " + auid3name + " as cloth.");
					}
					else {
						if (indexmodels < maxmodels) {
							auto assignedid = *next(freemodels.begin(), indexmodels);
							auid3sids[auid3name] = assignedid;
							++indexmodels;
							logassigned.writeText("Assigned regular model ID " + to_string(assignedid) + " to " + auid3name + ".");
						}
						else if (indexkillables < maxkillables) {
							auto assignedid = *next(freekillables.begin(), indexkillables);
							auid3sids[auid3name] = assignedid;
							++indexkillables;
							logassigned.writeText("Assigned killable model ID " + to_string(assignedid) + " to " + auid3name + " (forced).");
						}
						else {
							messagemodels = true;
							logassigned.writeText("Run out of free model IDs to assign any to " + auid3name + ".");
						}
					}
				}
			}

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
				FLAWrite(backupflaweapon.originalpath, "wpn:", true);

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
					auto flsfind = linesfiles.find("fls:");
					if (flsfind != linesfiles.end()) {
						for (auto flsline : flsfind->second) if (flsline.length() > 0) flslines.push_back(flsline);
						flsfind->second.clear();
					}
					flslines.push_back(";the end");
					Gflafile.open(backupflaaudio.originalpath, fstream::out);
					for (auto flsline : flslines) Gflafile << flsline << endl;
					Gflafile.close();
				};

				Events::initPoolsEvent.after += [] { FLAWrite(backupflamodel.originalpath, "spc:", true); };
			}

			for (auto filepath : linesfiles) {
				if (filepath.second.size() > 0) {
					auto linesfile = fstream();
					linesfile.open(filepath.first, fstream::out);
					if (linesfile.is_open()) {
						for (auto fileline : filepath.second) {
							linesfile << fileline << endl;
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

				#define FINDWEAPON  \
					auto weaponfind = weaponsauid3s.find(weaponid); \
					if (weaponfind != weaponsauid3s.end())
				#define FINDAUID3 \
					auto auid3find = idsauid3s.find(modelid); \
					if (auid3find != idsauid3s.end())
				#define FINDCLOTH \
					auto clothfind = clothesauid3s.find(clothhash); \
					if(clothfind != clothesauid3s.end())

				auto weaponslot = playerpointer->m_nActiveWeaponSlot;
				for (auto weapondata : playerpointer->m_aWeapons) {
					auto weaponid = weapondata.m_nType;
					FINDWEAPON {
						if (weapondata.m_nState != eWeaponState::WEAPONSTATE_OUT_OF_AMMO) {
							auto saveentity = SavePlayerWeapon(
									weaponfind->first
								,	weaponfind->second
								,	weapondata.m_nTotalAmmo
								,	weaponslot == CWeaponInfo::GetWeaponInfo(weaponid, 1)->m_nSlot
								,	weapondata.m_nAmmoInClip
							);
							saveplayerweapons.push_back(saveentity);
							playerpointer->ClearWeapon(weaponid);
						}
					}
				}

				for (auto gangid = int(0); gangid < gangsamount; ++gangid) {
					for (auto weaponindex = int(0); weaponindex < 3; ++weaponindex) {
						auto gangweapon = gangsWeapon(gangid, weaponindex);
						auto weaponid = patch::GETGANGSWEAPON(gangweapon, false);
						FINDWEAPON {
							auto saveentity = SaveGangWeapon(
									weaponid
								,	weaponfind->second
								,	gangid
								,	weaponindex
							);
							savegangweapons.push_back(saveentity);
							patch::SETGANGSWEAPON(gangweapon, eWeaponType::WEAPON_UNARMED, false);
						}
					}
				}

				CPickups::RemoveUnnecessaryPickups(CVector(0.0f, 0.0f, 0.0f), 99999.0f);
				for (auto pickupid = int(0); pickupid < pickupsamount; ++pickupid) {
					auto pickupmodel = pickupsModel(pickupid);
					auto modelid = patch::GETPICKUPSMODEL(pickupmodel);
					FINDAUID3 {
						auto saveentity = SavePickupModel(
								modelid
							,	auid3find->second
							,	pickupid
						);
						savepickupmodels.push_back(saveentity);
						patch::SETPICKUPSMODEL(pickupmodel, eModelID::MODEL_INFO);
					}
				}

				for (auto garageid = int(0); garageid < garagesamount; ++garageid) {
					auto garagemodel = garagesModel(garageid);
					auto modelid = patch::GETGARAGESMODEL(garagemodel, false);
					FINDAUID3 {
						auto saveentity = SaveGarageCar(
								modelid
							,	auid3find->second
							,	garageid
						);
						savegaragecars.push_back(saveentity);
						patch::SETGARAGESMODEL(garagemodel, eModelID::MODEL_NULL, false);
					}
				}

				for (auto generatorid = int(0); generatorid < generatorsamount; ++generatorid) {
					auto generatormodel = generatorsModel(generatorid);
					auto modelid = patch::GETGENERATORSMODEL(generatormodel);
					FINDAUID3 {
						auto saveentity = SaveGeneratorCar(
								modelid
							,	auid3find->second
							,	generatorid
						);
						savegeneratorcars.push_back(saveentity);
						patch::SETGENERATORSMODEL(generatormodel, -1);
					}
				}

				for (auto garageid = int(0); garageid < garagesamount; ++garageid) {
					for (auto modid = int(0); modid < modsamount; ++modid) {
						auto modmodel = modsModel(garageid, modid);
						auto modelid = patch::GETMODSMODEL(modmodel);
						FINDAUID3 {
							auto saveentity = SaveCarMod(
									modelid
								,	auid3find->second
								,	garageid
								,	modid
							);
							savecarmods.push_back(saveentity);
							patch::SETMODSMODEL(modmodel, -1, false);
						}
					}
				}

				#define CLEARLIST CLOTHESMODELS
				#define CLEARENTITIES saveclothmodels
				#define CLEARENTITY SaveClothModel
				#define CLOTHESCLEAR \
					{ \
						auto clothpart = int(0); \
						for (auto &clothhash : CLEARLIST) { \
							FINDCLOTH { \
								auto saveentity = CLEARENTITY( \
										0 \
									,	clothfind->second \
									,	clothpart \
								); \
								CLEARENTITIES.push_back(saveentity); \
								clothhash = 0; \
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
			};

			CdeclEvent <AddressList< //CGenericGameStorage::GenericSave
					0x619081, H_CALL //C_PcSave::SaveSlot
			>, PRIORITY_AFTER, ArgPickNone, void()> CGenericGameStorage__GenericSaveEvent;
			CGenericGameStorage__GenericSaveEvent += [] {
				savePaths(CGenericGameStorage::ms_SaveFileNameJustSaved);

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
					SAVEINI.WriteInteger(inistring, saveentity.sgarage, saveentity.garage);
					SAVEINI.WriteInteger(inistring, saveentity.smod, saveentity.mod);
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

				saveApply();
			};

			CdeclEvent <AddressList< //C3dMarkers::LoadUser3dMarkers
					0x5D19CE, H_CALL //GenericLoad
			>, PRIORITY_AFTER, ArgPickNone, void()> C3dMarkers__LoadUser3dMarkersEvent;
			C3dMarkers__LoadUser3dMarkersEvent += [] {
				savePaths(CGenericGameStorage::ms_LoadFileNameWithPath);

				auto initotal = int();

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
					, LOADINI.ReadInteger(inistring, e.sgarage, 0)
					, LOADINI.ReadInteger(inistring, e.smod, 0)
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

				saveApply();
			};

			if (messagemodels) modMessage("Run out of free model IDs.", MB_ICONERROR);
			if (messageweapons) modMessage("Run out of free weapon IDs.", MB_ICONERROR);
			if (messageunassigned) modMessage("Some AUID3s have unassigned IDs.", MB_ICONERROR);
		}
		else {
			modMessage(modname + " will not work for this session.", MB_ICONWARNING);
		}

		logmain.newLine();
		logmain.writeText("Approximate time taken: " + to_string((chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - timerstart)).count()) + "ms.");
//<
    }
//>
//<
} autoID3000;

//>
//<