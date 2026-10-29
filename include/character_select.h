#ifndef GUARD_CHARACTER_SELECT_H
#define GUARD_CHARACTER_SELECT_H

// Hero / Hina selection screen. Entered from main_menu.c in place of the
// "BOY or GIRL" menu. Sets gSaveBlock2Ptr->playerGender (MALE = Hero,
// FEMALE = Hina) and then continues to the naming screen.
void CB2_CharacterSelect(void);

// Implemented in main_menu.c: opens the naming screen and returns into the
// normal new-game flow afterwards.
void MainMenu_StartNamingScreenAfterCharacterSelect(void);

#endif // GUARD_CHARACTER_SELECT_H
