"""Optional documentation chart; pip install matplotlib."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parents[1];baseline=json.loads((root/'docs/benchmark-copy-baseline.json').read_text());current=json.loads((root/'docs/benchmark.json').read_text())
fig,ax=plt.subplots(figsize=(10,4.8),layout='constrained');positions=list(range(4));width=.35
ax.bar([x-width/2 for x in positions],[p['median_messages_per_second'] for p in baseline['profiles']],width,label='Whole-book copy',color='#9a6b32')
ax.bar([x+width/2 for x in positions],[p['median_messages_per_second'] for p in current['profiles']],width,label='Touched-node journal',color='#286b67')
ax.set_xticks(positions,[f"{p['target_active_depth']:,} orders\n{p['max_messages_per_packet']} msg/packet" for p in current['profiles']]);ax.set_yscale('log');ax.set(ylabel='Median messages/sec · logarithmic scale',title='Market Data Replay · same synthetic captures, same final state');ax.legend(frameon=False);ax.spines[['top','right']].set_visible(False);ax.grid(axis='y',alpha=.2)
fig.supxlabel('20,000 messages · three fresh processes per profile · local capture reads + validation + replay · no network',fontsize=9)
out=root/'docs/assets';out.mkdir(exist_ok=True,parents=True);fig.savefig(out/'replay-benchmark.png',dpi=160);fig.savefig(out/'replay-benchmark.svg')
