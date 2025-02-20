static char g_strCommandPrefix[][] = {
	"vsh",
	"vsh_",
	"hale",
	"hale_"
};

public void Command_Init()
{
	//Commands for everyone
	RegConsoleCmd("vsh", Command_MainMenu);
	RegConsoleCmd("hale", Command_MainMenu);
	
	Command_Create("menu", Command_MainMenu);
	Command_Create("class", Command_Weapon);
	Command_Create("weapon", Command_Weapon);
	Command_Create("boss", Command_Boss);
	Command_Create("modifiers", Command_Modifiers);
	Command_Create("next", Command_HaleNext);
	Command_Create("credits", Command_Credits);
	
	Command_Create("settings", Command_Preferences);
	Command_Create("preferences", Command_Preferences);
	Command_Create("bosstoggle", Command_Preferences_Boss);
	Command_Create("winstreak", Command_Preferences_Winstreak);
	Command_Create("duo", Command_Preferences_Duo);
	Command_Create("music", Command_Preferences_Music);
	Command_Create("revival", Command_Preferences_Revival);
	Command_Create("zombie", Command_Preferences_Revival);
	
	//Commands for admin only
	Command_Create("admin", Command_AdminMenu);
	Command_Create("refresh", Command_ConfigRefresh);
	Command_Create("cfg", Command_ConfigRefresh);
	Command_Create("queue", Command_AddQueuePoints);
	Command_Create("point", Command_AddQueuePoints);
	Command_Create("special", Command_ForceSpecialRound);
	Command_Create("dome", Command_ForceDome);
	Command_Create("rage", Command_SetRage);
}

stock void Command_Create(const char[] sCommand, ConCmd callback)
{
	for (int i = 0; i < sizeof(g_strCommandPrefix); i++)
	{
		char sBuffer[256];
		Format(sBuffer, sizeof(sBuffer), "%s%s", g_strCommandPrefix[i], sCommand);
		RegConsoleCmd(sBuffer, callback);
	}
}

public Action Command_MainMenu(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	Menu_DisplayMain(iClient);
	return Plugin_Handled;
}

public Action Command_Weapon(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	if (iArgs == 0)
	{
		MenuWeapon_DisplayMain(iClient);
		return Plugin_Handled;
	}

	char sClass[10];
	GetCmdArg(1, sClass, sizeof(sClass));
	TFClassType nClass = TF2_GetClassType(sClass);

	if (iArgs == 1)
	{
		MenuWeapon_DisplayClass(iClient, nClass);
		return Plugin_Handled;
	}

	char sSlot[10];
	GetCmdArg(2, sSlot, sizeof(sSlot));
	for (int iSlot = 0; iSlot < sizeof(g_strSlotName); iSlot++)
	{
		if (StrContains(g_strSlotName[iSlot], sSlot, false) != -1)
		{
			MenuWeapon_DisplaySlot(iClient, nClass, iSlot);
			return Plugin_Handled;
		}
	}
	
	//Slot name not found
	MenuWeapon_DisplayClass(iClient, nClass);
	return Plugin_Handled;
}

public Action Command_Boss(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	MenuBoss_DisplayBossMain(iClient);
	return Plugin_Handled;
}

public Action Command_Modifiers(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	MenuBoss_DisplayModifiersMain(iClient);
	return Plugin_Handled;
}

public Action Command_HaleNext(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	Menu_DisplayQueue(iClient);
	return Plugin_Handled;
}

public Action Command_Preferences(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	if (iArgs == 0)
	{
		//No args, just display prefs	
		Menu_DisplayPreferences(iClient);
		return Plugin_Handled;
	}
	else
	{
		char sPreferences[64];
		GetCmdArg(1, sPreferences, sizeof(sPreferences));
		
		for (int iPreferences = 0; iPreferences < sizeof(g_strPreferencesName); iPreferences++)
		{
			if (StrContains(g_strPreferencesName[iPreferences], sPreferences, false) == 0)
			{
				halePreferences preferences = view_as<halePreferences>(RoundToNearest(Pow(2.0, float(iPreferences))));
				
				bool bValue = !Preferences_Get(iClient, preferences);
				if (Preferences_Set(iClient, preferences, bValue))
				{
					char buffer[512];
					
					if (bValue)
						Format(buffer, sizeof(buffer), "Enable");
					else
						Format(buffer, sizeof(buffer), "Disable");
					
					PrintToChat(iClient, "%s%s %s %s", VSH_TAG, VSH_TEXT_COLOR, buffer, g_strPreferencesName[iPreferences]);
					return Plugin_Handled;
				}
				else
				{
					PrintToChat(iClient, "%s%s Your preferences is still loading, try again later.", VSH_TAG, VSH_ERROR_COLOR);
					return Plugin_Handled;
				}
			}
		}
		
		PrintToChat(iClient, "Invalid preferences entered", VSH_TAG, VSH_ERROR_COLOR);
		return Plugin_Handled;
	}
}

public Action Command_Preferences_Boss(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	ClientCommand(iClient, "vsh_preferences boss");
	return Plugin_Handled;
}

public Action Command_Preferences_Winstreak(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	ClientCommand(iClient, "vsh_preferences winstreak");
	return Plugin_Handled;
}

public Action Command_Preferences_Duo(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	ClientCommand(iClient, "vsh_preferences duo");
	return Plugin_Handled;
}

public Action Command_Preferences_Music(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	ClientCommand(iClient, "vsh_preferences music");
	return Plugin_Handled;
}

public Action Command_Preferences_Revival(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	ClientCommand(iClient, "vsh_preferences revival");
	return Plugin_Handled;
}

public Action Command_Credits(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	Menu_DisplayCredits(iClient);
	return Plugin_Handled;
}

public Action Command_AdminMenu(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (iClient == 0)
	{
		ReplyToCommand(iClient, "This Command can only be used ingame");
		return Plugin_Handled;
	}

	if (Client_HasFlag(iClient, haleClientFlags_Admin))
	{
		MenuAdmin_DisplayMain(iClient);
		return Plugin_Handled;
	}
	else
	{
		ReplyToCommand(iClient, "%s%s You do not have permission to use this command.", VSH_TAG, VSH_ERROR_COLOR);
		return Plugin_Handled;
	}
}

public Action Command_ConfigRefresh(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (Client_HasFlag(iClient, haleClientFlags_Admin))
	{
		Config_Refresh();
		
		PrintToChatAll("%s%s %N refreshed vsh config", VSH_TAG, VSH_TEXT_COLOR, iClient);
		return Plugin_Handled;
	}

	ReplyToCommand(iClient, "%s%s You do not have permission to use this command.", VSH_TAG, VSH_ERROR_COLOR);
	return Plugin_Handled;
}

public Action Command_AddQueuePoints(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (Client_HasFlag(iClient, haleClientFlags_Admin))
	{
		int iAddQueue;
		if (iArgs < 2)
		{
			ReplyToCommand(iClient, "%s%s Usage: vshqueue [target] [amount]", VSH_TAG, VSH_ERROR_COLOR);
			return Plugin_Handled;
		}
		
		char sArg1[10], sArg2[10];
		GetCmdArg(1, sArg1, sizeof(sArg1));
		GetCmdArg(2, sArg2, sizeof(sArg2));
		
		if (StringToIntEx(sArg2, iAddQueue) == 0)
		{
			ReplyToCommand(iClient, "%s%s Could not convert '%s' to int", VSH_TAG, VSH_ERROR_COLOR, sArg2);
			return Plugin_Handled;
		}
		
		int iTargetList[TF_MAXPLAYERS];
		char sTargetName[MAX_TARGET_LENGTH];
		bool bIsML;
		
		int iTargetCount = ProcessTargetString(sArg1, iClient, iTargetList, sizeof(iTargetList), COMMAND_FILTER_NO_IMMUNITY, sTargetName, sizeof(sTargetName), bIsML);
		if (iTargetCount <= 0)
		{
			ReplyToCommand(iClient, "%s%s Could not find anyone to give queue points", VSH_TAG, VSH_ERROR_COLOR);
			return Plugin_Handled;
		}
		
		for (int i = 0; i < iTargetCount; i++)
			Queue_AddPlayerPoints(iTargetList[i], iAddQueue);
		
		ReplyToCommand(iClient, "%s%s Gave %s %d queue points", VSH_TAG, VSH_TEXT_COLOR, sTargetName, iAddQueue);
		return Plugin_Handled;
	}

	ReplyToCommand(iClient, "%s%s You do not have permission to use this command.", VSH_TAG, VSH_ERROR_COLOR);
	return Plugin_Handled;
}

public Action Command_ForceSpecialRound(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (Client_HasFlag(iClient, haleClientFlags_Admin))
	{
		char sClass[256];
		
		if (iArgs < 1)
		{
			Format(sClass, sizeof(sClass), "random");
			g_bSpecialRound = true;
			g_nSpecialRoundNextClass = TFClass_Unknown;
		}
		else
		{
			GetCmdArg(1, sClass, sizeof(sClass));
			TFClassType nClass = TF2_GetClassType(sClass);
			
			if (nClass == TFClass_Unknown)
			{
				ReplyToCommand(iClient, "%s%s Unable to find class '%s'", VSH_TAG, VSH_ERROR_COLOR, sClass);
				return Plugin_Handled;
			}
			
			Format(sClass, sizeof(sClass), g_strClassName[nClass]);
			g_bSpecialRound = true;
			g_nSpecialRoundNextClass = nClass;
		}
		
		PrintToChatAll("%s%s %N force set next round a %s special round!", VSH_TAG, VSH_TEXT_COLOR, iClient, sClass);
		return Plugin_Handled;
	}

	ReplyToCommand(iClient, "%s%s You do not have permission to use this command.", VSH_TAG, VSH_ERROR_COLOR);
	return Plugin_Handled;
}

public Action Command_ForceDome(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (Client_HasFlag(iClient, haleClientFlags_Admin))
	{
		if (Dome_Start())
		{
			PrintToChatAll("%s%s %N force start the dome!", VSH_TAG, VSH_TEXT_COLOR, iClient);
			return Plugin_Handled;
		}
		else
		{
			ReplyToCommand(iClient, "%s%s There is already a active dome!", VSH_TAG, VSH_ERROR_COLOR);
			return Plugin_Handled;
		}
	}

	ReplyToCommand(iClient, "%s%s You do not have permission to use this command.", VSH_TAG, VSH_ERROR_COLOR);
	return Plugin_Handled;
}

public Action Command_SetRage(int iClient, int iArgs)
{
	if (!g_bEnabled) return Plugin_Continue;

	if (Client_HasFlag(iClient, haleClientFlags_Admin))
	{
		int iRage;
		if (iArgs == 0)
		{
			iRage = 100;
		}
		else if (iArgs == 1)
		{
			char strBuf[4];
			GetCmdArg(1, strBuf, sizeof(strBuf));
			if (StringToIntEx(strBuf, iRage) == 0)
			{
				ReplyToCommand(iClient, "%s%s Could not convert '%s' to int", VSH_TAG, VSH_ERROR_COLOR, strBuf);
				return Plugin_Handled;
			}
		}
		else
		{
			ReplyToCommand(iClient, "%s%s Usage: vsh_rage [amount=100]", VSH_TAG, VSH_ERROR_COLOR);
			return Plugin_Handled;
		}

		for (int i = 1; i <= TF_MAXPLAYERS; i++)
		{
			SaxtonHaleBase boss = SaxtonHaleBase(i);
			if (boss.bValid && boss.iMaxRageDamage != -1)
				boss.iRageDamage = RoundToNearest(float(boss.iMaxRageDamage) * (float(iRage)/100.0));
		}

		PrintToChatAll("%s%s %N sets rage to %i percent", VSH_TAG, VSH_TEXT_COLOR, iClient, iRage);
		return Plugin_Handled;
	}

	ReplyToCommand(iClient, "%s%s You do not have permission to use this command.", VSH_TAG, VSH_ERROR_COLOR);
	return Plugin_Handled;
}