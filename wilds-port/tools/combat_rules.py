"""GU combat rules with traced Wilds windows where documented."""
MOVES=[
 # label, source node, GU slot, source ID, template, cancel, recovery-end
 ('red_1','286',247,901,244,44,95),
 ('charge_release','212',248,902,147,105,145),
 ('move_forward','222',249,903,157,53,100),
 ('release','295',250,904,148,126,205),
 ('focus_start','460',251,905,144,68,94),
 ('red_2','288',208,906,244,53,108),
 ('red_3','289',209,907,244,77,122),
 ('charge_start','279',210,908,147,0,228),
 ('charge_hold','281_Loop',211,909,147,0,126),
 ('charge_walk','275_Loop',217,915,147,0,107),
 ('charge_round_prep','321',218,916,147,0,26),
 ('charge_round','213',219,917,148,55,100),
 ('red_foresight','290',220,918,157,80,160),
 ('move_right','224',212,910,157,48,92),
 ('move_left','223',213,911,157,48,92),
 ('move_back','225',214,912,157,78,115),
 ('focus_slash','466',215,913,144,84,85),
 ('focus_finish','467',216,914,144,42,90),
 ('red_1_stationary','285',98,920,244,44,95),
 ('red_1_retreat','287',99,921,244,90,95),
 ('spirit_2_forward','216',84,922,157,60,96),
 ('spirit_2_left','205',85,923,157,68,106),
 ('spirit_2_right','206',86,924,157,68,106),
 ('spirit_2_back','207',87,925,157,75,112),
]
# source, begin, end, motion values (four values are charge levels)
STRIKES=[
 (901,14,36,[24]),(906,20,48,[28]),(907,18,29,[12]),(907,42,70,[32]),
 (902,13,24,[12,16,20,24]),(902,33,45,[10,14,18,22]),(902,84,97,[16,20,26,32]),
 (903,30,47,[24]),(910,26,41,[24]),(911,26,41,[24]),(912,25,43,[20]),(912,60,71,[12]),
 # Wilds MOT932 AttackCollision_PlWp request53/54/55; request56 is
 # the real-contact sampler used by the delayed release shells.
 (904,23,33,[5]),(904,37,47,[5]),(904,81,94,[5]),
 (917,24,44,[42]),
 (918,15,35,[22]),(918,40,69,[13]),
 (905,44,62,[14]),(913,8,21,[12]),(913,36,49,[12]),(914,10,33,[28]),
]

SHARED_STRIKES=[
 # MOT932 RequestSet 35/36. Reuse GU adaptation damage rows 12/28.
 (922,28,39,907),(922,42,56,906),
 (923,20,38,907),(923,56,65,906),
 (924,20,38,907),(924,56,65,906),
 (925,22,36,907),(925,60,72,906),
]


# Wilds MOT AttackCollision_PlWp, half-open intervals. The final-cut
# auxiliary colliders 703-705 overlap the last cut in their respective clips.
# Motion values are the primary request sets in the Wilds wp03_attack RCOL.
# source, donor source, (begin, end, motion value) per cut
CRIMSON_STRIKES=[
 (901,901,[(16,25,11),(34,43,22)]),             # 286: 44,45
 (906,906,[(28,36,11),(42,50,22)]),             # 288: 44,45
 (907,907,[(21,29,30),(45,52,18),(60,70,55)]),  # 289: 46,47,48
 (920,901,[(14,22,33),(32,40,16)]),             # 285: 41,42
 (921,901,[(26,38,30),(46,54,18),(73,86,55)]),  # 287: 46,47,48
]

# Wilds full-gauge Spirit I–III, empty-gauge Spirit I, and Roundslash
# below red / at red. Clone the GU donors so the stock rows stay intact.
SPIRIT_NATIVE_ROWS=[
 (16,16,8,31),(17,17,9,14),
 (23,24,10,30),
 (26,27,11,14),(26,27,12,19),(26,27,13,34),
]
SPIRIT_ROUND_ROWS=[('NORMAL',161,38),('RED_FORESIGHT',106,42)]
