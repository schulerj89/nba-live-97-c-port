#include "frontend_help.h"
#include <string.h>

static int16_t lower(int a, int b) { return (int16_t)(a < b ? a : b); }
static int16_t upper(int a, int b) { return (int16_t)(a > b ? a : b); }

Nba97HelpEvent nba97_help_open(Nba97HelpModal *m, Nba97HelpRect target, uint16_t mask) {
    if (!m || m->phase != NBA97_HELP_CLOSED || !mask || target.x < 0 || target.y < 0 ||
        target.x > 246 || target.y > 110 || target.width < 20 || target.height < 10 ||
        target.x + target.width > 512 || target.y + target.height > 240)
        return NBA97_HELP_NO_EVENT;
    memset(m, 0, sizeof(*m));
    m->target = target;
    m->rect.x = 246; m->rect.y = 110; m->rect.width = 20; m->rect.height = 10;
    m->phase = NBA97_HELP_GROWING;
    m->held = mask;
    return NBA97_HELP_OPEN_SOUND; /* 8002F124(7), not warning sound 5. */
}

Nba97HelpEvent nba97_help_input(Nba97HelpModal *m, uint16_t raw) {
    if (!m) return NBA97_HELP_NO_EVENT;
    if (m->phase == NBA97_HELP_RETURN_BARRIER) {
        if (raw != m->held) {
            m->phase = NBA97_HELP_CLOSED;
            return NBA97_HELP_RETURNED;
        }
    } else {
        if (m->phase == NBA97_HELP_WAIT_CHANGE && raw != m->held)
            m->phase = NBA97_HELP_READY;
        if (m->phase == NBA97_HELP_READY && raw) {
            m->held = raw;
            m->phase = NBA97_HELP_SHRINKING;
            return NBA97_HELP_CLOSE_SOUND; /* 8002F124(8), text removed first. */
        }
    }
    return NBA97_HELP_NO_EVENT;
}

Nba97HelpEvent nba97_modal_open_prior(Nba97HelpModal* m,Nba97HelpRect rect,uint16_t prior) {
    const Nba97HelpEvent event=nba97_help_open(m,rect,prior ? prior:1);
    if(event==NBA97_HELP_OPEN_SOUND) m->held=prior;
    return event;
}

Nba97HelpEvent nba97_help_tick(Nba97HelpModal *m, uint16_t raw) {
    if (!m) return NBA97_HELP_NO_EVENT;
    if (m->phase == NBA97_HELP_GROWING) {
        m->rect.x = upper(m->target.x, m->rect.x - 9);
        m->rect.y = upper(m->target.y, m->rect.y - 4);
        m->rect.width = lower(m->target.width, m->rect.width + 18);
        m->rect.height = lower(m->target.height, m->rect.height + 8);
        if (m->rect.x == m->target.x && m->rect.y == m->target.y &&
            m->rect.width == m->target.width && m->rect.height == m->target.height)
            m->phase = NBA97_HELP_WAIT_CHANGE;
        return NBA97_HELP_NO_EVENT;
    }
    if (m->phase == NBA97_HELP_SHRINKING) {
        m->rect.x = lower(246, m->rect.x + 9);
        m->rect.y = lower(110, m->rect.y + 4);
        m->rect.width = upper(20, m->rect.width - 18);
        m->rect.height = upper(10, m->rect.height - 8);
        if (m->rect.x == 246 && m->rect.y == 110 && m->rect.width == 20 && m->rect.height == 10)
            m->phase = NBA97_HELP_RETURN_BARRIER;
        return NBA97_HELP_NO_EVENT;
    }
    return nba97_help_input(m, raw);
}

int nba97_help_visible(const Nba97HelpModal *m) {
    return m && m->phase != NBA97_HELP_CLOSED && m->phase != NBA97_HELP_RETURN_BARRIER;
}
int nba97_help_text_visible(const Nba97HelpModal *m) {
    return m && (m->phase == NBA97_HELP_WAIT_CHANGE || m->phase == NBA97_HELP_READY);
}
