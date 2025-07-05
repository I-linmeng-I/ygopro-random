#include "deck_manager.h"
#include "game.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm> // For std::shuffle, std::copy_if
#include <vector>
#include <random>
#include <unordered_set>
#include "myfilesystem.h"
#include "network.h"aa

namespace ygo {

#ifndef YGOPRO_SERVER_MODE
char DeckManager::deckBuffer[0x10000]{};
#endif
DeckManager deckManager;

void DeckManager::LoadLFListSingle(const char* path) {
	auto cur = _lfList.rend();
	FILE* fp = myfopen(path, "r");
	char linebuf[256]{};
	wchar_t strBuffer[256]{};
	char str1[16]{};
	if(fp) {
		while(std::fgets(linebuf, sizeof linebuf, fp)) {
			if(linebuf[0] == '#')
				continue;
			if(linebuf[0] == '!') {
				auto len = std::strcspn(linebuf, "\r\n");
				linebuf[len] = 0;
				BufferIO::DecodeUTF8(&linebuf[1], strBuffer);
				LFList newlist;
				newlist.listName = strBuffer;
				newlist.hash = 0x7dfcee6a;
				_lfList.push_back(newlist);
				cur = _lfList.rbegin();
				continue;
			}
			if (cur == _lfList.rend())
				continue;
			unsigned int code = 0;
			int count = -1;
			if (std::sscanf(linebuf, "%10s%*[ ]%1d", str1, &count) != 2)
				continue;
			if (count < 0 || count > 2)
				continue;
			code = std::strtoul(str1, nullptr, 10);
			cur->content[code] = count;
			cur->hash = cur->hash ^ ((code << 18) | (code >> 14)) ^ ((code << (27 + count)) | (code >> (5 - count)));
		}
		std::fclose(fp);
	}
}
void DeckManager::LoadLFList() {
#ifdef SERVER_PRO2_SUPPORT
	LoadLFListSingle("config/lflist.conf");
#endif
	LoadLFListSingle("expansions/lflist.conf");
	LoadLFListSingle("lflist.conf");
	LFList nolimit;
	nolimit.listName = L"N/A";
	nolimit.hash = 0;
	_lfList.push_back(nolimit);
}
const wchar_t* DeckManager::GetLFListName(unsigned int lfhash) {
	auto lit = std::find_if(_lfList.begin(), _lfList.end(), [lfhash](const ygo::LFList& list) {
		return list.hash == lfhash;
	});
	if(lit != _lfList.end())
		return lit->listName.c_str();
	return dataManager.unknown_string;
}
const LFList* DeckManager::GetLFList(unsigned int lfhash) {
	auto lit = std::find_if(_lfList.begin(), _lfList.end(), [lfhash](const ygo::LFList& list) {
		return list.hash == lfhash;
	});
	if (lit != _lfList.end())
		return &(*lit);
	return nullptr;
}
static unsigned int checkAvail(unsigned int ot, unsigned int avail) {
	if((ot & avail) == avail)
		return 0;
	if((ot & AVAIL_OCG) && (avail != AVAIL_OCG))
		return DECKERROR_OCGONLY;
	if((ot & AVAIL_TCG) && (avail != AVAIL_TCG))
		return DECKERROR_TCGONLY;
	return DECKERROR_NOTAVAIL;
}
unsigned int DeckManager::CheckDeck(const Deck& deck, unsigned int lfhash, int rule) {
	std::unordered_map<int, int> ccount;
	auto list = GetLFListContent(lfhash);
	if(!list)
		return 0;
	int dc = 0;
	// if(deck.main.size() < 40 || deck.main.size() > 60)
	// 	return (DECKERROR_MAINCOUNT << 28) + deck.main.size();
	// if(deck.extra.size() > 15)
	// 	return (DECKERROR_EXTRACOUNT << 28) + deck.extra.size();
	if(deck.side.size() > 15)
		return (DECKERROR_SIDECOUNT << 28) + deck.side.size();
	const int rule_map[6] = { AVAIL_OCG, AVAIL_TCG, AVAIL_SC, AVAIL_CUSTOM, AVAIL_OCGTCG, 0 };
	int avail = rule_map[rule];
	for(size_t i = 0; i < deck.main.size(); ++i) {
		code_pointer cit = deck.main[i];
		int gameruleDeckError = checkAvail(cit->second.ot, avail);
		if(gameruleDeckError)
			return (gameruleDeckError << 28) | cit->first;
		if (cit->second.type & (TYPES_EXTRA_DECK | TYPE_TOKEN))
			return (DECKERROR_MAINCOUNT << 28);
		int code = cit->second.alias ? cit->second.alias : cit->first;
		ccount[code]++;
		int dc = ccount[code];
		if(dc > 3)
			return (DECKERROR_CARDCOUNT << 28) | cit->first;
		auto it = list.find(code);
		if(it != list.end() && dc > it->second)
			return (DECKERROR_LFLIST << 28) | cit->first;
	}
	for (auto& cit : deck.extra) {
		auto gameruleDeckError = checkAvail(cit->second.ot, avail);
		if(gameruleDeckError)
			return (gameruleDeckError << 28) | cit->first;
		if (!(cit->second.type & TYPES_EXTRA_DECK) || cit->second.type & TYPE_TOKEN)
			return (DECKERROR_EXTRACOUNT << 28);
		int code = cit->second.alias ? cit->second.alias : cit->first;
		ccount[code]++;
		int dc = ccount[code];
		if(dc > 3)
			return (DECKERROR_CARDCOUNT << 28) | cit->first;
		auto it = list.find(code);
		if(it != list.end() && dc > it->second)
			return (DECKERROR_LFLIST << 28) | cit->first;
	}
	for (auto& cit : deck.side) {
		auto gameruleDeckError = checkAvail(cit->second.ot, avail);
		if(gameruleDeckError)
			return (gameruleDeckError << 28) | cit->first;
		if (cit->second.type & TYPE_TOKEN)
			return (DECKERROR_SIDECOUNT << 28);
		int code = cit->second.alias ? cit->second.alias : cit->first;
		ccount[code]++;
		int dc = ccount[code];
		if(dc > 3)
			return (DECKERROR_CARDCOUNT << 28) | cit->first;
		auto it = list.find(code);
		if(it != list.end() && dc > it->second)
			return (DECKERROR_LFLIST << 28) | cit->first;
	}
	return 0;
}
int DeckManager::LoadDeck(Deck& deck, int* dbuf, int mainc, int sidec, bool is_packlist) {
	auto &allCardsData = dataManager.GetAllCardsData(); // 获取所有卡片数据
	std::vector<unsigned int> mainCodes, extraCodes;

	std::unordered_set<unsigned int> banlist;
	std::ifstream banlistfile("Iflist.txt");
	unsigned int code;
	while (banlistfile >> code)
	{
		banlist.insert(code);
	}

	// 筛选卡片代码
	for (const auto &card : allCardsData)
	{
		if (!(card.second.type & TYPE_TOKEN) && banlist.find(card.first) == banlist.end())
		{ // 排除Token类型和禁止的卡
			if (card.second.type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ | TYPE_LINK))
			{
				extraCodes.push_back(card.first); // 适合额外卡组的卡
			}
			else
			{
				mainCodes.push_back(card.first); // 适合主卡组和副卡组的卡
			}
		}
	}

	// 随机选择卡片
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(mainCodes.begin(), mainCodes.end(), g);
	std::shuffle(extraCodes.begin(), extraCodes.end(), g);

	// 为了简化，我们将副卡组也从mainCodes中选择
	std::vector<unsigned int> sideCodes(mainCodes.begin(), mainCodes.end());

	// 选择卡片数量
	int mainDeckSize = std::min(60, static_cast<int>(mainCodes.size()));
	int extraDeckSize = std::min(30, static_cast<int>(extraCodes.size()));
	int sideDeckSize = std::min(0, static_cast<int>(sideCodes.size()));

	deck.clear();
	int errorcode = 0;

	// 加载主卡组
	for (int i = 0; i < mainDeckSize; ++i)
	{
		auto code = mainCodes[i];
		CardData cd;
		if (!dataManager.GetData(code, &cd))
		{
			errorcode = code;
			continue;
		}
		deck.main.push_back(dataManager.GetCodePointer(code));
	}

	// 加载额外卡组
	for (int i = 0; i < extraDeckSize; ++i)
	{
		auto code = extraCodes[i];
		CardData cd;
		if (!dataManager.GetData(code, &cd))
		{
			errorcode = code;
			continue;
		}
		deck.extra.push_back(dataManager.GetCodePointer(code));
	}

	// 加载副卡组
	for (int i = 0; i < sideDeckSize; ++i)
	{
		auto code = sideCodes[i];
		CardData cd;
		if (!dataManager.GetData(code, &cd))
		{
			errorcode = code;
			continue;
		}
		deck.side.push_back(dataManager.GetCodePointer(code));
	}

	// 加入规则卡
	std::string filePath = "./ruleCardList.txt"; // 指定文件路径
	std::ifstream file(filePath);
	std::string line;

	if (file.is_open())
	{
		// 读取文件中的每一行
		while (std::getline(file, line))
		{
			std::istringstream iss(line);
			std::string part;
			std::vector<long> numbers;

			// 假设数字之间以空格分隔
			while (std::getline(iss, part, ' '))
			{
				try
				{
					long number = std::stol(part);
					numbers.push_back(number);
				}
				catch (std::invalid_argument &e)
				{
					std::cerr << "Conversion error: " << e.what() << '\n';
				}
				catch (std::out_of_range &e)
				{
					std::cerr << "Number out of range: " << e.what() << '\n';
				}
			}

			// 输出转换后的数字
			for (long num : numbers)
			{
				deck.main.push_back(dataManager.GetCodePointer(num));
			}
		}
	}

	return errorcode;
}
uint32_t DeckManager::LoadDeckFromStream(Deck& deck, std::istringstream& deckStream, bool is_packlist) {
	int ct = 0;
	int mainc = 0, sidec = 0;
	uint32_t cardlist[PACK_MAX_SIZE]{};
	bool is_side = false;
	std::string linebuf;
	while (std::getline(deckStream, linebuf, '\n') && ct < PACK_MAX_SIZE) {
		if (linebuf[0] == '!') {
			is_side = true;
			continue;
		}
		if (linebuf[0] < '0' || linebuf[0] > '9')
			continue;
		auto code = std::strtoul(linebuf.c_str(), nullptr, 10);
		if (code >= UINT32_MAX)
			continue;
		cardlist[ct++] = code;
		if (is_side)
			++sidec;
		else
			++mainc;
	}
	return LoadDeck(deck, cardlist, mainc, sidec, is_packlist);
}
bool DeckManager::LoadSide(Deck& deck, uint32_t dbuf[], int mainc, int sidec) {
	std::unordered_map<uint32_t, int> pcount;
	std::unordered_map<uint32_t, int> ncount;
	for(size_t i = 0; i < deck.main.size(); ++i)
		pcount[deck.main[i]->first]++;
	for(size_t i = 0; i < deck.extra.size(); ++i)
		pcount[deck.extra[i]->first]++;
	for(size_t i = 0; i < deck.side.size(); ++i)
		pcount[deck.side[i]->first]++;
	Deck ndeck;
	LoadDeck(ndeck, dbuf, mainc, sidec);
	// if(ndeck.main.size() != deck.main.size() || ndeck.extra.size() != deck.extra.size())
	// 	return false;
	// for(size_t i = 0; i < ndeck.main.size(); ++i)
	// 	ncount[ndeck.main[i]->first]++;
	// for(size_t i = 0; i < ndeck.extra.size(); ++i)
	// 	ncount[ndeck.extra[i]->first]++;
	// for(size_t i = 0; i < ndeck.side.size(); ++i)
	// 	ncount[ndeck.side[i]->first]++;
	// for(auto cdit = ncount.begin(); cdit != ncount.end(); ++cdit)
	// 	if(cdit->second != pcount[cdit->first])
	// 		return false;
	deck = ndeck;
	return true;
}
#ifndef YGOPRO_SERVER_MODE
void DeckManager::GetCategoryPath(wchar_t* ret, int index, const wchar_t* text) {
	wchar_t catepath[256];
	switch(index) {
	case 0:
		myswprintf(catepath, L"./pack");
		break;
	case 1:
		BufferIO::CopyWideString(mainGame->gameConf.bot_deck_path, catepath);
		break;
	case -1:
	case 2:
	case 3:
		myswprintf(catepath, L"./deck");
		break;
	default:
		myswprintf(catepath, L"./deck/%ls", text);
	}
	BufferIO::CopyWStr(catepath, ret, 256);
}
void DeckManager::GetDeckFile(wchar_t* ret, int category_index, const wchar_t* category_name, const wchar_t* deckname) {
	wchar_t filepath[256];
	wchar_t catepath[256];
	if(deckname != nullptr) {
		GetCategoryPath(catepath, category_index, category_name);
		myswprintf(filepath, L"%ls/%ls.ydk", catepath, deckname);
		BufferIO::CopyWStr(filepath, ret, 256);
	}
	else {
		BufferIO::CopyWStr(L"", ret, 256);
	}
}
FILE* DeckManager::OpenDeckFile(const wchar_t* file, const char* mode) {
	FILE* fp = mywfopen(file, mode);
	return fp;
}
irr::io::IReadFile* DeckManager::OpenDeckReader(const wchar_t* file) {
#ifdef _WIN32
	auto reader = DataManager::FileSystem->createAndOpenFile(file);
#else
	char file2[256];
	BufferIO::EncodeUTF8(file, file2);
	auto reader = DataManager::FileSystem->createAndOpenFile(file2);
#endif
	return reader;
}
bool DeckManager::LoadCurrentDeck(std::istringstream& deckStream, bool is_packlist) {
	LoadDeckFromStream(current_deck, deckStream, is_packlist);
	return true;  // the above LoadDeck has return value but we ignore it here for now
}
bool DeckManager::LoadCurrentDeck(const wchar_t* file, bool is_packlist) {
	current_deck.clear();
	auto reader = OpenDeckReader(file);
	if(!reader) {
		wchar_t localfile[256];
		myswprintf(localfile, L"./deck/%ls.ydk", file);
		reader = OpenDeckReader(localfile);
	}
	if(!reader && !mywcsncasecmp(file, L"./pack", 6)) {
		wchar_t zipfile[256];
		myswprintf(zipfile, L"%ls", file + 2);
		reader = OpenDeckReader(zipfile);
	}
	if(!reader)
		return false;
	std::memset(deckBuffer, 0, sizeof deckBuffer);
	int size = reader->read(deckBuffer, sizeof deckBuffer);
	reader->drop();
	if (size >= (int)sizeof deckBuffer) {
		return false;
	}
	std::istringstream deckStream(deckBuffer);
	LoadDeckFromStream(current_deck, deckStream, is_packlist);
	return true;  // the above function has return value but we ignore it here for now
}
bool DeckManager::LoadCurrentDeck(int category_index, const wchar_t* category_name, const wchar_t* deckname) {
	wchar_t filepath[256];
	GetDeckFile(filepath, category_index, category_name, deckname);
	bool is_packlist = (category_index == 0);
	bool res = LoadCurrentDeck(filepath, is_packlist);
	if (res && mainGame->is_building)
		mainGame->deckBuilder.RefreshPackListScroll();
	return res;
}
void DeckManager::SaveDeck(const Deck& deck, std::stringstream& deckStream) {
	deckStream << "#created by ..." << std::endl;
	deckStream << "#main" << std::endl;
	for(size_t i = 0; i < deck.main.size(); ++i)
		deckStream << deck.main[i]->first << std::endl;
	deckStream << "#extra" << std::endl;
	for(size_t i = 0; i < deck.extra.size(); ++i)
		deckStream << deck.extra[i]->first << std::endl;
	deckStream << "!side" << std::endl;
	for(size_t i = 0; i < deck.side.size(); ++i)
		deckStream << deck.side[i]->first << std::endl;
}
bool DeckManager::SaveDeck(const Deck& deck, const wchar_t* file) {
	if(!FileSystem::IsDirExists(L"./deck") && !FileSystem::MakeDir(L"./deck"))
		return false;
	FILE* fp = OpenDeckFile(file, "w");
	if(!fp)
		return false;
	std::stringstream deckStream;
	SaveDeck(deck, deckStream);
	std::fwrite(deckStream.str().c_str(), 1, deckStream.str().length(), fp);
	std::fclose(fp);
	return true;
}
bool DeckManager::DeleteDeck(const wchar_t* file) {
	return FileSystem::RemoveFile(file);
}
bool DeckManager::CreateCategory(const wchar_t* name) {
	if(!FileSystem::IsDirExists(L"./deck") && !FileSystem::MakeDir(L"./deck"))
		return false;
	if(name[0] == 0)
		return false;
	wchar_t localname[256];
	myswprintf(localname, L"./deck/%ls", name);
	return FileSystem::MakeDir(localname);
}
bool DeckManager::RenameCategory(const wchar_t* oldname, const wchar_t* newname) {
	if(!FileSystem::IsDirExists(L"./deck") && !FileSystem::MakeDir(L"./deck"))
		return false;
	if(newname[0] == 0)
		return false;
	wchar_t oldlocalname[256];
	wchar_t newlocalname[256];
	myswprintf(oldlocalname, L"./deck/%ls", oldname);
	myswprintf(newlocalname, L"./deck/%ls", newname);
	return FileSystem::Rename(oldlocalname, newlocalname);
}
bool DeckManager::DeleteCategory(const wchar_t* name) {
	wchar_t localname[256];
	myswprintf(localname, L"./deck/%ls", name);
	if(!FileSystem::IsDirExists(localname))
		return false;
	return FileSystem::DeleteDir(localname);
}
bool DeckManager::SaveDeckArray(const DeckArray& deck, const wchar_t* name) {
	if (!FileSystem::IsDirExists(L"./deck") && !FileSystem::MakeDir(L"./deck"))
		return false;
	FILE* fp = OpenDeckFile(name, "w");
	if (!fp)
		return false;
	std::fprintf(fp, "#created by ...\n#main\n");
	for (const auto& code : deck.main)
		std::fprintf(fp, "%u\n", code);
	std::fprintf(fp, "#extra\n");
	for (const auto& code : deck.extra)
		std::fprintf(fp, "%u\n", code);
	std::fprintf(fp, "!side\n");
	for (const auto& code : deck.side)
		std::fprintf(fp, "%u\n", code);
	std::fclose(fp);
	return true;
}
#endif //YGOPRO_SERVER_MODE
}
