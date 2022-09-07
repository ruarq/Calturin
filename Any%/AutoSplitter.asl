state("Calturin")
{
	// GameManager variables
	// Just for reference: byte8 GameManager : "UnityPlayer.dll", 0x01768380, 0x88, 0xE70, 0x210, 0xF8, 0x78, 0x38;
	float bossesDefeated	: "UnityPlayer.dll", 0x01768380, 0x88, 0xE70, 0x210, 0xF8, 0x78, 0x38, 0x31c;
	bool handsOff			: "UnityPlayer.dll", 0x01768380, 0x88, 0xE70, 0x210, 0xF8, 0x78, 0x38, 0x321;
	bool newGame			: "UnityPlayer.dll", 0x01768380, 0x88, 0xE70, 0x210, 0xF8, 0x78, 0x38, 0x324;
	
	// PlayerScript variables
	// bool normalExekiasDefeated : "UnityPlayer.dll", 0x01768380, 0x88, 0xE70, 0x210, 0xF8, 0x78, 0x38, 0x20, 0x459;

	// ExekiasScript variables
	// pointer to the exekiasTakeDamage; nullptr when not fighting Exekias
	bool exekiasDead		: "UnityPlayer.dll", 0x01768380, 0x88, 0xE70, 0x210, 0xF8, 0x78, 0x38, 0x20, 0x408, 0x172;
}

start
{
	return !current.handsOff && old.handsOff && current.newGame;
}

split
{
	if (current.bossesDefeated != old.bossesDefeated)
	{
		// Such a beautiful return statement :)
		return
			// Arche
			current.bossesDefeated == 1.0f ||

			// Path of pain
			current.bossesDefeated == 2.0f ||

			// Moloch
			current.bossesDefeated == 3.0f ||

			// The Ossarium
			current.bossesDefeated == 4.0f ||

			// Sphere of the true path
			current.bossesDefeated == 5.0f ||

			// Topeth
			current.bossesDefeated == 6.0f ||

			// Togon
			current.bossesDefeated == 7.0f ||

			// Swamp of pain
			current.bossesDefeated == 8.0f ||

			// Sir Lanval
			current.bossesDefeated == 9.0f ||

			// Legion
			current.bossesDefeated == 10.0f;
	}

	// Exekias
	return current.exekiasDead;
}

reset
{
	return
		// After arche
		(current.bossesDefeated == 0.0f && old.bossesDefeated > 0.0f) ||

		// Before Arche is defeated, newGame is still true, so if bossesDefeated == 0.0f and newGame is set to false,
		// we're back in the main menu
		(current.bossesDefeated == 0.0f && !current.newGame);
}