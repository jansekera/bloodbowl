import sys, os
KOREN = sys.argv[1]; nasi, sou, g = sys.argv[2], sys.argv[3], int(sys.argv[4])
D = "/home/jenda/claude/blood-bowl/evidence/p233_20261028"
sys.path.insert(0, KOREN); sys.path.insert(0, os.path.join(D, "..", "p230_20261027"))
sys.argv = [sys.argv[0], KOREN, "x.pkl", "1"]
import bb_kontrola_tahu as kt, cilene
bb = kt.bb
s = cilene.pozice(bb, kt, nasi, sou, g)
kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=70000 + 100 * g)
for k in kroky: print("KROK", k["action"].type.name, file=sys.stderr)
