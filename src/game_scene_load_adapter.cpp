#include "game_scene_load_adapter.h"

#include <cstdint>
#include <cstring>

int nba97_game_scene_load_registers_from_session(
    const Nba97GameMatchSessionEvent* event,
    Nba97GameSceneLoadRegisters* registers) {
    if (!event || !registers ||
        event->kind != NBA97_GAME_MATCH_SESSION_LOAD_SCENE ||
        event->pc != UINT32_C(0x8002da84) ||
        event->entry != UINT32_C(0x8002db68) || event->argument_count != 0)
        return NBA97_TEXT_ARGUMENT;
    std::memset(registers, 0, sizeof *registers);
    registers->gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
        event->stack_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_GP] = {
        event->global_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_RA] = {
        event->return_address, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
        event->saved_register[0], 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {
        event->saved_register[1], 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = {
        event->saved_register[2], 0x0f};
    return NBA97_TEXT_COMPLETE;
}
