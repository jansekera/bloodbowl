import gzip, json
g = json.load(gzip.open("blitzlanding_replic_20260825_corpus_data/g0000.json.gz"))
print(list(g.keys()))
t = g["turn_logs"][5]
print({k: (v if not isinstance(v, list) else "list[%d]" % len(v)) for k, v in t.items()})
print(t["home_players"][0])
print(t["events"][:3])
print(len(g["turn_logs"]))
