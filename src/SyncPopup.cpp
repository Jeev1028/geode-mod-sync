#include "SyncPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

namespace {
constexpr char const* HEADER = "GEODESYNC1";
constexpr char const* API = "https://api.geode-sdk.org/v1";

struct Entry {
    std::string id;
    std::string version;
};

// Shared state for a running import; processed one mod at a time.
struct ImportJob {
    std::vector<Entry> entries;
    size_t next = 0;
    int installed = 0;
    std::vector<std::string> skipped;
    Ref<SyncPopup> popup;
};

std::vector<Entry> parseCode(std::string const& text) {
    std::vector<Entry> out;
    std::istringstream ss(text);
    std::string line;
    bool header = false;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        if (!header) {
            if (line != HEADER) return {};
            header = true;
            continue;
        }
        auto at = line.find('@');
        if (at == std::string::npos) continue;
        out.push_back({line.substr(0, at), line.substr(at + 1)});
    }
    return out;
}

void step(std::shared_ptr<ImportJob> job);

void finish(std::shared_ptr<ImportJob> job) {
    std::string msg = fmt::format("Installed {} mod(s).", job->installed);
    if (!job->skipped.empty()) {
        msg += fmt::format("\nSkipped {}:\n", job->skipped.size());
        for (auto& s : job->skipped) msg += s + "\n";
    }
    msg += "\nRestart the game to load them.";
    job->popup->setStatus("Done! Restart the game.");
    FLAlertLayer::create("Mod Sync", msg, "OK")->show();
}

void skip(std::shared_ptr<ImportJob> job, Entry const& e, std::string why) {
    job->skipped.push_back(fmt::format("{} ({})", e.id, why));
    step(job);
}

std::string extractLink(web::WebResponse& res) {
    if (!res.ok()) return "";
    auto body = res.string();
    if (!body.isOk()) return "";
    std::string text = body.unwrap();
    auto parsed = matjson::parse(text);
    if (!parsed.isOk()) return "";
    matjson::Value root = parsed.unwrap();
    if (!root.contains("payload")) return "";
    matjson::Value payload = root["payload"];
    if (!payload.contains("download_link")) return "";
    return payload["download_link"].asString().unwrapOr("");
}

void onFile(std::shared_ptr<ImportJob> job, Entry e, web::WebResponse file) {
    if (!file.ok()) {
        skip(job, e, "download failed");
        return;
    }
    auto path = dirs::getModsDir() / (e.id + ".geode");
    if (file.into(path).isErr()) {
        skip(job, e, "couldn't save file");
        return;
    }
    job->installed++;
    step(job);
}

void onInfo(std::shared_ptr<ImportJob> job, Entry e, web::WebResponse res) {
    std::string link = extractLink(res);
    if (link.empty()) {
        skip(job, e, "no build for this device / not on index");
        return;
    }
    async::spawn(
        web::WebRequest().userAgent("Geode Mod Sync").followRedirects(true).get(link),
        [job, e](web::WebResponse file) { onFile(job, e, std::move(file)); }
    );
}

void step(std::shared_ptr<ImportJob> job) {
    if (job->next >= job->entries.size()) {
        finish(job);
        return;
    }
    Entry e = job->entries[job->next++];
    job->popup->setStatus(fmt::format("{}/{}: {}", job->next, job->entries.size(), e.id));

    // Already have this version (or newer)? nothing to do.
    if (auto mod = Loader::get()->getInstalledMod(e.id)) {
        auto want = VersionInfo::parse(e.version);
        if (want.isOk() && mod->getVersion() >= want.unwrap()) {
            step(job);
            return;
        }
    }

    auto url = fmt::format("{}/mods/{}/versions/{}?gd={}&platforms={}", API, e.id, e.version,
                           Loader::get()->getGameVersion(), GEODE_PLATFORM_SHORT_IDENTIFIER);
    async::spawn(
        web::WebRequest().userAgent("Geode Mod Sync").get(url),
        [job, e](web::WebResponse res) { onInfo(job, e, std::move(res)); }
    );
}
}  // namespace

SyncPopup* SyncPopup::create() {
    auto ret = new SyncPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool SyncPopup::init() {
    if (!Popup::init(320.f, 200.f)) return false;
    this->setTitle("Mod Sync");

    auto info = CCLabelBMFont::create("Copy your mod list here, then import it\non another device.", "chatFont.fnt");
    info->setScale(0.8f);
    m_mainLayer->addChildAtPosition(info, Anchor::Top, ccp(0.f, -48.f));

    auto menu = CCMenu::create();
    menu->setContentWidth(280.f);
    menu->addChild(CCMenuItemSpriteExtra::create(ButtonSprite::create("Copy Mod List"), this,
                                                 menu_selector(SyncPopup::onExport)));
    menu->addChild(CCMenuItemSpriteExtra::create(ButtonSprite::create("Import"), this,
                                                 menu_selector(SyncPopup::onImport)));
    menu->setLayout(RowLayout::create()->setGap(10.f));
    m_mainLayer->addChildAtPosition(menu, Anchor::Center, ccp(0.f, -5.f));

    m_status = CCLabelBMFont::create("", "bigFont.fnt");
    m_status->setScale(0.4f);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Bottom, ccp(0.f, 28.f));
    return true;
}

void SyncPopup::setStatus(std::string const& text) {
    if (m_status) m_status->setString(text.c_str());
}

void SyncPopup::onExport(CCObject*) {
    std::string out = std::string(HEADER) + "\n";
    int n = 0;
    for (auto mod : Loader::get()->getAllMods()) {
        if (mod->isInternal()) continue;
        out += fmt::format("{}@{}\n", std::string(mod->getID()), mod->getVersion().toNonVString());
        n++;
    }
    if (utils::clipboard::write(out)) {
        this->setStatus(fmt::format("Copied {} mods!", n));
        FLAlertLayer::create("Copied",
                             fmt::format("{} mods copied to clipboard.\nSend it to your other device, copy it there, then tap Import.", n),
                             "OK")->show();
    } else {
        this->setStatus("Clipboard write failed");
    }
}

void SyncPopup::onImport(CCObject*) {
    auto entries = parseCode(utils::clipboard::read());
    if (entries.empty()) {
        FLAlertLayer::create("Nothing to import",
                             "Clipboard doesn't contain a Mod Sync list. Copy the whole text first.", "OK")->show();
        return;
    }
    auto job = std::make_shared<ImportJob>();
    job->entries = std::move(entries);
    job->popup = this;
    step(job);
}
