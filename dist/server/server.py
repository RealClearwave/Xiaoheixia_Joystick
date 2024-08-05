from flask import Flask, request
import vgamepad

app = Flask(__name__)
gamepad = vgamepad.VX360Gamepad()

keydict = {
    9: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_X,
    1: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_A,
    2: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_B,
    3: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_Y,
    4: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_START,
    5: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_UP,
    6: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_DOWN,
    7: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_LEFT,
    8: vgamepad.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_RIGHT,
}
key = 0

@app.route('/upload', methods=['GET'])
def upload():
    global key
    x = min(max(0.0,float(request.args.get('x'))),100.0)
    y = min(max(0.0,float(request.args.get('y'))),100.0)
    a = min(max(0.0,float(request.args.get('a'))),100.0)
    z = min(max(0.0,float(request.args.get('z'))),100.0)
    rot = min(max(0.0,float(request.args.get('rot'))),100.0)
    t_key = int(request.args.get('key'))
    gamepad.left_joystick_float((x-50)/50, (y-50)/50)
    gamepad.right_joystick_float((rot-50)/50, (z-50)/50)
    gamepad.left_trigger_float(max(0,(a-50)/50))
    gamepad.right_trigger_float(max(0,-(a-50)/50))

    if t_key != key:
        if key != 0:
            gamepad.release_button(keydict[key])
        if t_key != 0:
            gamepad.press_button(keydict[t_key])
        key = t_key

    gamepad.update()
    return f'x={x}, y={y}, a={a}, z={z}, rot={rot}, key={key}'

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
