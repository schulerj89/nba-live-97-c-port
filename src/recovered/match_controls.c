#include "match_controls.h"
#include <string.h>

int nba97_match_controls_finalize(Nba97MatchControls* live,const int8_t selectors[8],
        const Nba97ProfileControls* profiles,const uint8_t defaults[59],int force,
        uint8_t provenance[8]) {
    unsigned c;
    if(!live || !selectors || !profiles || !defaults || !provenance) return 0;
    if(!force) for(c=0;c<8;++c) if(selectors[c]>=20) return 0;
    for(c=0;c<8;++c) {
        const uint8_t* source=0;
        memset(live->stats[c],0,36); /* 8A944 length0x24, not0x24 halfwords. */
        provenance[c]=NBA97_CONTROLS_RETAINED;
        if(force || (selectors[c]>=0 && !profiles->valid[(unsigned)selectors[c]])) {
            source=defaults;provenance[c]=NBA97_CONTROLS_DEFAULT;
        } else if(selectors[c]>=0) {
            source=profiles->map[(unsigned)selectors[c]];provenance[c]=NBA97_CONTROLS_PROFILE;
        }
        if(source) memcpy(live->map[c],source,59);
    }
    return 1;
}
