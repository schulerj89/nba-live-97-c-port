#include "create_player_store.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static void editor_acceptance(const fs::path& root) {
    using Status=nba97::CreatedPlayerAcceptStatus;
    const auto path=root/"acceptance.n97cpl";
    Nba97CreatedPlayerCatalog catalog{};
    nba97::CreatedPlayerStore store;
    store.load(path,catalog);
    Nba97CreateEditor editor{};
    assert(nba97_create_editor_open_new(&editor,&catalog));
    const auto empty=editor;
    assert(store.acceptEditor(editor,catalog)==Status::Invalid);
    assert(!std::memcmp(&editor,&empty,sizeof(editor)) && store.generation()==0);
    std::strcpy(editor.first_name,"Save"); std::strcpy(editor.last_name,"Test");
    assert(store.acceptEditor(editor,catalog)==Status::Written);
    assert(!editor.txn.active && store.generation()==1);
    const auto timestamp=fs::last_write_time(path);
    assert(nba97_create_editor_open_edit(&editor,&catalog,0));
    assert(store.acceptEditor(editor,catalog)==Status::Unchanged);
    assert(!editor.txn.active && store.generation()==1 && fs::last_write_time(path)==timestamp);
    assert(!fs::exists(fs::path(path.wstring()+L".bak")));
    assert(nba97_create_editor_open_edit(&editor,&catalog,0));
    editor.selected_field=NBA97_CREATE_JERSEY_NUMBER;
    assert(nba97_create_editor_adjust(&editor,-1) && editor.jersey_number==99);
    assert(nba97_create_editor_adjust(&editor,1) && editor.jersey_number==0);
    assert(store.acceptEditor(editor,catalog)==Status::Unchanged && store.generation()==1);

    assert(nba97_create_editor_open_edit(&editor,&catalog,0));
    editor.jersey_number=7;
    const auto before_editor=editor;
    const auto before_catalog=catalog;
    // Force actual I/O failure. Unlike a no-op, it must retain the active draft.
    const auto temp=fs::path(path.wstring()+L".tmp");
    fs::create_directory(temp);
    bool failed=false;
    try { store.acceptEditor(editor,catalog); } catch(const std::exception&) { failed=true; }
    assert(failed && !std::memcmp(&editor,&before_editor,sizeof(editor)) &&
           !std::memcmp(&catalog,&before_catalog,sizeof(catalog)) && store.generation()==1);
    fs::remove(temp);
    assert(store.acceptEditor(editor,catalog)==Status::Written && store.generation()==2);
    assert(!editor.txn.active);
    nba97::CreatedPlayerStore reload;
    Nba97CreatedPlayerCatalog restored{};
    assert(reload.load(path,restored)==nba97::CreatedPlayerLoadStatus::Loaded);
    assert(restored.records[0].raw[7]==7 && reload.generation()==2);
}

int main() {
    const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root=fs::temp_directory_path()/fs::path("nba97-create-player-"+unique);
    const auto path=root/"players.n97cpl";
    fs::create_directories(root);
    try {
        editor_acceptance(root);
        Nba97CreatedPlayerCatalog catalog{};
        nba97::CreatedPlayerStore store;
        assert(store.load(path,catalog)==nba97::CreatedPlayerLoadStatus::NewStore);
        assert(store.generation()==0 && nba97_created_count(&catalog)==0);

        Nba97CreateEditor editor{};
        assert(nba97_create_editor_open_new(&editor,&catalog));
        assert(nba97_create_editor_append_letter(&editor,'j'));
        assert(nba97_create_editor_move(&editor,1));
        assert(nba97_create_editor_append_letter(&editor,'d'));
        assert(nba97_create_editor_move(&editor,1));
        assert(nba97_create_editor_adjust(&editor,1));
        assert(nba97_create_editor_save(&editor,&catalog));
        assert(store.save(catalog) && store.generation()==1);
        assert(!store.save(catalog) && store.generation()==1);

        Nba97CreatedPlayerCatalog restored{};
        nba97::CreatedPlayerStore reload;
        assert(reload.load(path,restored)==nba97::CreatedPlayerLoadStatus::Loaded);
        assert(reload.generation()==1 && !std::memcmp(&catalog,&restored,sizeof(catalog)));
        Nba97CreateEditor reopened{};
        assert(nba97_create_editor_open_edit(&reopened,&restored,0));
        assert(std::string(reopened.first_name)=="J" && std::string(reopened.last_name)=="D");
        assert(reopened.team==1);
        assert(restored.metadata[0].roster_slot==5);

        assert(nba97_created_delete(&restored,0));
        assert(reload.save(restored) && reload.generation()==2);
        assert(fs::exists(fs::path(path.wstring()+L".bak")));

        { std::fstream corrupt(path,std::ios::binary|std::ios::in|std::ios::out); assert(corrupt);
          corrupt.seekp(40); const char bad='X'; corrupt.write(&bad,1); }
        Nba97CreatedPlayerCatalog recovered{};
        nba97::CreatedPlayerStore fallback;
        assert(fallback.load(path,recovered)==nba97::CreatedPlayerLoadStatus::RecoveredBackup);
        assert(fallback.generation()==1 && nba97_created_count(&recovered)==1);
    } catch(...) { fs::remove_all(root); throw; }
    fs::remove_all(root);
    return 0;
}
