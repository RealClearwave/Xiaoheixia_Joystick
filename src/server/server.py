from flask import Flask, request
import vgamepad

app = Flask(__name__)
gamepad = vgamepad.VX360Gamepad()

@app.route('/upload', methods=['GET'])
def upload():
    x = min(max(0.0,float(request.args.get('x'))),100.0)
    y = min(max(0.0,float(request.args.get('y'))),100.0)
    a = min(max(0.0,float(request.args.get('a'))),100.0)
    z = min(max(0.0,float(request.args.get('z'))),100.0)
    gamepad.left_joystick_float((x-50)/50, (y-50)/50)
    gamepad.right_joystick_float((a-50)/50, (z-50)/50)
    gamepad.update()
    return f'x={x}, y={y}, a={a}, z={z}'

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
