/* Generated from executed original DOS text builders. */
#ifndef MERCS_DEBRIEF_VIEW_FIXTURES_H
#define MERCS_DEBRIEF_VIEW_FIXTURES_H
typedef struct { unsigned state,priority; int time; const char *text; } mdv_row_fixture;
typedef struct { mercs_debrief_terms terms; unsigned count; mdv_row_fixture rows[6]; unsigned kills[2]; const char *objectives,*finances; } mdv_report_fixture;
static const mdv_report_fixture mdv_report_cases[] = {
{{.mode=0,.restore_flag=0,.campaign_type=1,.funds=1000000,.success_pay=12000,.failure_pay=2400,.target_pay=17,.bonus_pay=31,.bounty_pay=43,.expense_factor=3,.per_mech_a=2,.per_mech_b=5,.fixed_a=7,.fixed_b=11,.fixed_c=13,.pilot_pay=101,.aero_pay=103,.owned_mech_count=2,.success_next="NEXT",.failure_next="FAIL"},0,{{0,0,0,""}},{0,0},"Time\\g040Type\\g110Objective\\g400Status\\g514\\b000\\n\\n\\n\\g110Total confirmed kills: 0","Funds: \\g514\\b0541,000,000\\nContract compensation: \\g514\\b03612,000\\nTotal Income: \\g514\\b03612,000\\nTotal expenses:  \\g514\\b024-750\\n\\g514\\b084--------------\\nBalance: \\g514\\b0541,011,250\\n"},
{{.mode=0,.restore_flag=0,.campaign_type=1,.funds=1000000,.success_pay=12000,.failure_pay=2400,.target_pay=17,.bonus_pay=31,.bounty_pay=43,.expense_factor=3,.per_mech_a=2,.per_mech_b=5,.fixed_a=7,.fixed_b=11,.fixed_c=13,.pilot_pay=101,.aero_pay=103,.owned_mech_count=2,.success_next="NEXT",.failure_next="FAIL"},6,{{1,1,74,"Target  one"},{0,2,-1,"Bonus\tfailed"},{2,4,1,"Bounty"},{1,8,80,"Return"},{1,0,2," Default "},{1,3,20,"Unknown"}},{3,4},"Time\\g040Type\\g110Objective\\g400Status\\g514\\b000\\n00:01\\g040Bounty\\g110Bounty\\g514\\b000\\g400Unknown\\n00:02\\g040Default\\g110 Default \\g400Successful\\n00:20\\g040Unknown\\g110Unknown\\g514\\b000\\g400Successful\\n01:14\\g040Target\\g110Target one\\g514\\b000\\g400Successful\\n01:20\\g040Return\\g110Return\\g514\\b000\\g400Successful\\nDNF\\g040Bonus\\g110Bonus failed\\g514\\b000\\g400Failed\\n\\n\\n\\g110Total confirmed kills: 7","Funds: \\g514\\b0541,000,000\\nContract compensation: \\g514\\b03612,000\\nTotal Income: \\g514\\b03612,060\\nTotal expenses:  \\g514\\b024-750\\n\\g514\\b084--------------\\nBalance: \\g514\\b0541,011,310\\n"},
{{.mode=0,.restore_flag=0,.campaign_type=1,.funds=1000000,.success_pay=12000,.failure_pay=2400,.target_pay=17,.bonus_pay=31,.bounty_pay=43,.expense_factor=3,.per_mech_a=2,.per_mech_b=5,.fixed_a=7,.fixed_b=11,.fixed_c=13,.pilot_pay=101,.aero_pay=103,.owned_mech_count=2,.success_next="NEXT",.failure_next="FAIL"},2,{{1,1,3602,"Minute wraps"},{0,2,4,"Second"}},{1,0},"Time\\g040Type\\g110Objective\\g400Status\\g514\\b000\\n00:04\\g040Bonus\\g110Second\\g514\\b000\\g400Failed\\n00:02\\g040Target\\g110Minute wraps\\g514\\b000\\g400Successful\\n\\n\\n\\g110Total confirmed kills: 1","Funds: \\g514\\b0541,000,000\\nContract compensation: \\g514\\b03612,000\\nTotal Income: \\g514\\b03612,017\\nTotal expenses:  \\g514\\b024-750\\n\\g514\\b084--------------\\nBalance: \\g514\\b0541,011,267\\n"},
{{.mode=1,.restore_flag=0,.campaign_type=1,.funds=1000000,.success_pay=12000,.failure_pay=2400,.target_pay=17,.bonus_pay=31,.bounty_pay=43,.expense_factor=3,.per_mech_a=2,.per_mech_b=5,.fixed_a=7,.fixed_b=11,.fixed_c=13,.pilot_pay=101,.aero_pay=103,.owned_mech_count=2,.success_next="NEXT",.failure_next="FAIL"},0,{{0,0,0,""}},{0,0},"Time\\g040Type\\g110Objective\\g400Status\\g514\\b036Reward\\n","Funds: \\g514\\b0541,000,000\\nContract compensation: \\g514\\b03612,000\\nTotal Income: \\g514\\b03612,000\\nTotal expenses:  \\g514\\b024-750\\n\\g514\\b084--------------\\nBalance: \\g514\\b0541,011,250\\n"},
{{.mode=1,.restore_flag=0,.campaign_type=1,.funds=1000000,.success_pay=12000,.failure_pay=2400,.target_pay=17,.bonus_pay=31,.bounty_pay=43,.expense_factor=3,.per_mech_a=2,.per_mech_b=5,.fixed_a=7,.fixed_b=11,.fixed_c=13,.pilot_pay=101,.aero_pay=103,.owned_mech_count=2,.success_next="NEXT",.failure_next="FAIL"},6,{{1,1,74,"Target  one"},{0,2,-1,"Bonus\tfailed"},{2,4,1,"Bounty"},{1,8,80,"Return"},{1,0,2," Default "},{1,3,20,"Unknown"}},{3,4},"Time\\g040Type\\g110Objective\\g400Status\\g514\\b036Reward\\n00:01\\g040Bounty\\g110Bounty\\g514\\b01243\\g400Unknown\\n00:02\\g040Default\\g110 Default \\g400Successful\\n00:20\\g040Unknown\\g110Unknown\\g514\\b0060\\g400Successful\\n01:14\\g040Target\\g110Target one\\g514\\b01217\\g400Successful\\n01:20\\g040Return\\g110Return\\g514\\b0060\\g400Successful\\nDNF\\g040Bonus\\g110Bonus failed\\g514\\b0060\\g400Failed\\n\\n\\g110Cash bounty for 7 confirmed kills\\g514\\b018301","Funds: \\g514\\b0541,000,000\\nContract compensation: \\g514\\b03612,000\\nTotal Income: \\g514\\b03612,361\\nTotal expenses:  \\g514\\b024-750\\n\\g514\\b084--------------\\nBalance: \\g514\\b0541,011,611\\n"},
{{.mode=1,.restore_flag=0,.campaign_type=1,.funds=1000000,.success_pay=12000,.failure_pay=2400,.target_pay=17,.bonus_pay=31,.bounty_pay=43,.expense_factor=3,.per_mech_a=2,.per_mech_b=5,.fixed_a=7,.fixed_b=11,.fixed_c=13,.pilot_pay=101,.aero_pay=103,.owned_mech_count=2,.success_next="NEXT",.failure_next="FAIL"},2,{{1,1,3602,"Minute wraps"},{0,2,4,"Second"}},{1,0},"Time\\g040Type\\g110Objective\\g400Status\\g514\\b036Reward\\n00:04\\g040Bonus\\g110Second\\g514\\b0060\\g400Failed\\n00:02\\g040Target\\g110Minute wraps\\g514\\b01217\\g400Successful\\n\\n\\g110Cash bounty for 1 confirmed kill\\g514\\b01243","Funds: \\g514\\b0541,000,000\\nContract compensation: \\g514\\b03612,000\\nTotal Income: \\g514\\b03612,060\\nTotal expenses:  \\g514\\b024-750\\n\\g514\\b084--------------\\nBalance: \\g514\\b0541,011,310\\n"},
};
typedef struct { unsigned ni,nm; int items[3][3]; unsigned flags[3]; const char *text; } mdv_salvage_fixture;
static const mdv_salvage_fixture mdv_salvage_cases[] = {
{0,0,{{0,0,0}},{0},"\\nThere were no weapons salvaged.\\n\\nThere were no mechs salvaged.\\n"},
{0,1,{{0,0,0}},{0},"\\nThere were no weapons salvaged.\\n\\nThere were no mechs salvaged.\\n"},
{0,1,{{0,0,0}},{1},"\\nThere were no weapons salvaged.\\n\\nSalvaged Mechs:\\n\\tMECH0\\n"},
{0,3,{{0,0,0}},{1,0,1},"\\nThere were no weapons salvaged.\\n\\nSalvaged Mechs:\\n\\tMECH0, MECH1\\n"},
{0,3,{{0,0,0}},{0,1,1},"\\nThere were no weapons salvaged.\\n\\nSalvaged Mechs:\\n\\tMECH0, \\n"},
{0,3,{{0,0,0}},{1,1,0},"\\nThere were no weapons salvaged.\\n\\nSalvaged Mechs:\\n\\n"},
{0,3,{{0,0,0}},{1,1,1},"\\nThere were no weapons salvaged.\\n\\nSalvaged Mechs:\\n\\tMECH0, MECH1, MECH2\\n"},
{1,0,{{1,0,1}},{0},"\\nSalvaged weapons:\\n\\t1 LRM 15 (C)\\nThere were no mechs salvaged.\\n"},
{1,1,{{1,0,1}},{0},"\\nSalvaged weapons:\\n\\t1 LRM 15 (C)\\nThere were no mechs salvaged.\\n"},
{1,1,{{1,0,1}},{1},"\\nSalvaged weapons:\\n\\t1 LRM 15 (C)\\nSalvaged Mechs:\\n\\tMECH0\\n"},
{1,3,{{1,0,1}},{1,0,1},"\\nSalvaged weapons:\\n\\t1 LRM 15 (C)\\nSalvaged Mechs:\\n\\tMECH0, MECH1\\n"},
{1,3,{{1,0,1}},{0,1,1},"\\nSalvaged weapons:\\n\\t1 LRM 15 (C)\\nSalvaged Mechs:\\n\\tMECH0, \\n"},
{1,3,{{1,0,1}},{1,1,0},"\\nSalvaged weapons:\\n\\t1 LRM 15 (C)\\nSalvaged Mechs:\\n\\n"},
{1,3,{{1,0,1}},{1,1,1},"\\nSalvaged weapons:\\n\\t1 LRM 15 (C)\\nSalvaged Mechs:\\n\\tMECH0, MECH1, MECH2\\n"},
{3,0,{{1,0,1},{1,0,1},{2,1,1}},{0},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), 1 tons (LRM 10 (C)) ammo, \\nThere were no mechs salvaged.\\n"},
{3,1,{{1,0,1},{1,0,1},{2,1,1}},{0},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), 1 tons (LRM 10 (C)) ammo, \\nThere were no mechs salvaged.\\n"},
{3,1,{{1,0,1},{1,0,1},{2,1,1}},{1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), 1 tons (LRM 10 (C)) ammo, \\nSalvaged Mechs:\\n\\tMECH0\\n"},
{3,3,{{1,0,1},{1,0,1},{2,1,1}},{1,0,1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), 1 tons (LRM 10 (C)) ammo, \\nSalvaged Mechs:\\n\\tMECH0, MECH1\\n"},
{3,3,{{1,0,1},{1,0,1},{2,1,1}},{0,1,1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), 1 tons (LRM 10 (C)) ammo, \\nSalvaged Mechs:\\n\\tMECH0, \\n"},
{3,3,{{1,0,1},{1,0,1},{2,1,1}},{1,1,0},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), 1 tons (LRM 10 (C)) ammo, \\nSalvaged Mechs:\\n\\n"},
{3,3,{{1,0,1},{1,0,1},{2,1,1}},{1,1,1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), 1 tons (LRM 10 (C)) ammo, \\nSalvaged Mechs:\\n\\tMECH0, MECH1, MECH2\\n"},
{2,0,{{1,2,1},{2,1,1}},{0},"\\nSalvaged weapons:\\n\\t, 1 tons (LRM 10 (C)) ammo\\nThere were no mechs salvaged.\\n"},
{2,1,{{1,2,1},{2,1,1}},{0},"\\nSalvaged weapons:\\n\\t, 1 tons (LRM 10 (C)) ammo\\nThere were no mechs salvaged.\\n"},
{2,1,{{1,2,1},{2,1,1}},{1},"\\nSalvaged weapons:\\n\\t, 1 tons (LRM 10 (C)) ammo\\nSalvaged Mechs:\\n\\tMECH0\\n"},
{2,3,{{1,2,1},{2,1,1}},{1,0,1},"\\nSalvaged weapons:\\n\\t, 1 tons (LRM 10 (C)) ammo\\nSalvaged Mechs:\\n\\tMECH0, MECH1\\n"},
{2,3,{{1,2,1},{2,1,1}},{0,1,1},"\\nSalvaged weapons:\\n\\t, 1 tons (LRM 10 (C)) ammo\\nSalvaged Mechs:\\n\\tMECH0, \\n"},
{2,3,{{1,2,1},{2,1,1}},{1,1,0},"\\nSalvaged weapons:\\n\\t, 1 tons (LRM 10 (C)) ammo\\nSalvaged Mechs:\\n\\n"},
{2,3,{{1,2,1},{2,1,1}},{1,1,1},"\\nSalvaged weapons:\\n\\t, 1 tons (LRM 10 (C)) ammo\\nSalvaged Mechs:\\n\\tMECH0, MECH1, MECH2\\n"},
{3,0,{{1,0,1},{1,1,1},{3,0,0}},{0},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), \\nThere were no mechs salvaged.\\n"},
{3,1,{{1,0,1},{1,1,1},{3,0,0}},{0},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), \\nThere were no mechs salvaged.\\n"},
{3,1,{{1,0,1},{1,1,1},{3,0,0}},{1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), \\nSalvaged Mechs:\\n\\tMECH0\\n"},
{3,3,{{1,0,1},{1,1,1},{3,0,0}},{1,0,1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), \\nSalvaged Mechs:\\n\\tMECH0, MECH1\\n"},
{3,3,{{1,0,1},{1,1,1},{3,0,0}},{0,1,1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), \\nSalvaged Mechs:\\n\\tMECH0, \\n"},
{3,3,{{1,0,1},{1,1,1},{3,0,0}},{1,1,0},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), \\nSalvaged Mechs:\\n\\n"},
{3,3,{{1,0,1},{1,1,1},{3,0,0}},{1,1,1},"\\nSalvaged weapons:\\n\\t2 LRM 15 (C), \\nSalvaged Mechs:\\n\\tMECH0, MECH1, MECH2\\n"},
};
typedef struct { int32_t value; const char *text; } mdv_money_fixture;
static const mdv_money_fixture mdv_money_cases[] = {
{0,"0"},
{1,"1"},
{-1,"-1"},
{999,"999"},
{1000,"1,000"},
{-1000,"-1,000"},
{1000000,"1,000,000"},
{-2147483648,"-2,147,483,648"},
{2147483647,"2,147,483,647"},
};
#endif
