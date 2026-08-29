#include "create_player_store.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root=fs::temp_directory_path()/fs::path("nba97-create-player-"+unique);
    const auto path=root/"players.n97cpl";
    fs::create_directories(root);
    try {
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
