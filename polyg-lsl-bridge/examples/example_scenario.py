"""Minimal scenario that emits LSL markers at stimulus boundaries.

Run the bridge and (optionally) the fake device in other terminals, start
LabRecorder, then run this to see markers land alongside the EEG stream.
"""
import time

from polyg_lsl.markers import MarkerStream

STEPS = [
    ("scenario/onset", 1.0),
    ("choice1/onset", 1.5),
    ("choice2/onset", 1.5),
    ("iti/onset", 1.0),
]


def main() -> None:
    mk = MarkerStream(name="EEG_Scenario_Markers", source_id="example-scenario-1")
    print("pushing markers; start your recorder now...")
    time.sleep(2.0)
    for label, dur in STEPS:
        mk.push(label)
        print(f"marker: {label}")
        time.sleep(dur)
    print("done")


if __name__ == "__main__":
    main()
