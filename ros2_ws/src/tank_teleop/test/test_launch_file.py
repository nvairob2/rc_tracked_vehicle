from pathlib import Path


def test_bringup_launch_exists():
    path = Path(__file__).resolve().parents[2] / 'tank_bringup' / 'launch' / 'teleop.launch.py'
    text = path.read_text(encoding='utf-8')
    assert 'tank_teleop' in text
    assert '8888' in text
    assert 'micro_ros_agent' in text
