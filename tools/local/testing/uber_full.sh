#!/bin/sh
# 2026-11-10: the full ubershader check on all of the reference playthrough (snatcherLeo: "do all of that stuff now, and then, just did the
# entire the reference playthrough again with ubershader ... I don't mind"; record: docs/port/pipeline-hitches.md). One sitting at a time:
#   GEN  generated shaders (ubershader off), frames sampled every 3,000 frames: the picture reference
#   UB   ubershader forced on, empty cache, the same frames, cache kept: its pipelines become the shipped list
#        (tools/local/port/pipeline_seed.py merges the 17 caches)
#   UBS  ubershader on, starting only from the shipped list (no cache): any slow frame without disc reads after
#        start-up would be a shader build stall
#   HY   the hybrid ubershader (U13 on), from the shipped list, the same frames as GEN (2026-11-11)
# Checks: each pass's roll record identical to GEN's (cmp of replay.rolls, so the three sittings that already differ
# from their live recording are compared too), frames GEN vs UB pixel-identical (framediff.py, threshold 1), UBS slow
# frames all explained. Moved into the repo from a session scratch folder on 2026-11-11.
# usage: sh tools/local/testing/uber_full.sh <GEN|UB|UBS|HY> <exe> [seed db for UBS / HY, or -] [pass name, default UF_<mode>]
# Summaries and per-sitting outputs go to $UBER_OUT (default _run3/checks/uber_full); UI_FROM defaults to the
# hub-trial's port/ui (the uber exes are hub builds).
HERE=$(cd "$(dirname "$0")" && pwd)
# Folders from this script's place, not one PC's absolute paths (2026-11-24, for the public repo; the same folders as
# before on this PC): ORRE_WORK = the sittings and check folders (default: _run3 beside the repo, as run3_pc_replay.sh)
BESIDE=$(cd "$HERE/../../../.." && pwd)
R=${ORRE_WORK:-$BESIDE/_run3}
OUTD=${UBER_OUT:-$R/checks/uber_full}
UI=${UI_FROM:-$BESIDE/hub-trial/port/ui}
mkdir -p "$OUTD"
MODE=$1
EXE=$2
SEED=$3
PASS=${4:-UF_$MODE}
SUM=$OUTD/uber_full_$PASS.summary
: > $SUM
cd $R
for s in P03_20261103_165021 P01_20261102_204956 P02_20261103_113532 P04_20261103_231906 P05_20261103_234643 \
         P06_20261104_192303 P07_20261105_003307 P08_20261105_160716 P09_20261105_213103 P10A_20261106_004339 \
         P10_20261106_012539 P11_20261106_192252 P12_20261106_232843 P13_20261107_220659 P14_20261109_130716 \
         P15_20261109_151347 P16_20261110_083537; do
    extra=""
    [ "$s" = "P02_20261103_113532" ] && extra="ORRE_REPLAY_COMPAT=cry60"
    frames=$(python -I -c "print(','.join(str(f) for f in range(3000, 400000, 3000)))")
    case $MODE in
        # ORRE_UBERSHADER=0: the ubershader is on by default from U11 (2026-11-11); older exes ignore the 0
        GEN) EXE_FROM=$EXE UI_FROM=$UI sh $HERE/run3_pc_replay.sh $s $PASS $extra ORRE_DUMP_FRAMES=$frames \
                 ORRE_UBERSHADER=0 > $OUTD/replay_${PASS}_$s.out 2>&1 ;;
        UB) KEEP_CACHE=1 EXE_FROM=$EXE UI_FROM=$UI sh $HERE/run3_pc_replay.sh $s $PASS $extra \
                ORRE_DUMP_FRAMES=$frames ORRE_UBERSHADER=1 > $OUTD/replay_${PASS}_$s.out 2>&1 ;;
        UBS) SEED_FROM=$SEED EXE_FROM=$EXE UI_FROM=$UI sh $HERE/run3_pc_replay.sh $s $PASS $extra \
                 ORRE_UBERSHADER=1 > $OUTD/replay_${PASS}_$s.out 2>&1 ;;
        # HY (2026-11-11, U13 on): the hybrid ubershader (the default) from the launch list, frames as GEN's
        HY) SEED_FROM=$SEED EXE_FROM=$EXE UI_FROM=$UI sh $HERE/run3_pc_replay.sh $s $PASS $extra \
                ORRE_DUMP_FRAMES=$frames ORRE_UBERSHADER=2 > $OUTD/replay_${PASS}_$s.out 2>&1 ;;
    esac
    rolls=$(grep "== rolls:" $OUTD/replay_${PASS}_$s.out | tail -1)
    [ -z "$rolls" ] && rolls=$(tail -1 $OUTD/replay_${PASS}_$s.out)
    same="(GEN itself)"
    if [ "$MODE" != "GEN" ]; then
        if cmp -s $R/checks/${s}_UF_GEN/replay.rolls $R/checks/${s}_$PASS/replay.rolls; then same="rolls = GEN"; else same="ROLLS DIFFER FROM GEN"; fi
    fi
    log=$(ls $R/checks/${s}_$PASS/logs/*.log 2>/dev/null | head -1)
    slow=$(grep -ac "^\[slowframe\]" "$log" 2>/dev/null)
    nodisc=$(grep -a "^\[slowframe\]" "$log" 2>/dev/null | grep -c "disc reads started 0")
    uber=$(grep -a "uber-fragment:" "$log" 2>/dev/null | tail -1 | sed 's/.*uber-fragment: //; s/\]$//')
    echo "$s | $rolls | $same | slow $slow (no disc reads $nodisc) | $uber" >> $SUM
done
echo "all done" >> $SUM
cat $SUM
