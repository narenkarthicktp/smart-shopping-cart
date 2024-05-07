from time import sleep
from flask import Flask, Response, render_template, request,redirect
from numpy import frombuffer, uint8
import stripe
import yolo_img
import json

app = Flask(__name__, static_url_path='/static')
detection_result_queue = []

YOUR_DOMAIN = 'http://localhost:3000'

stripe.api_key = 'sk_test_51M4AXqSJIWtlP70ggcilmoIswX2RDNToUwhsKDFmj3QSTKxn66JSXMXNPn2vOw5zMgpUBiQGGBvomOjKkYXvU2cm004NmXK3vk'
items_list = {
    "Pen": {
        "image" : "https://images.pexels.com/photos/372748/pexels-photo-372748.jpeg",
        "price" : 10,
        "priceId": "price_1OwEOuSJIWtlP70gvh5bAUU2",
        "description" : "Finest Quality pen from Germany"
    },
    "Matchbox": {
        "image" : "https://images.pexels.com/photos/1602905/pexels-photo-1602905.jpeg",
        "price" : 2,
        "priceId" : "price_1Ow4fiSJIWtlP70gj6apCosP",
        "description" : "50 matches in a box,"
    },
    "Sharpener": {
        "image" : "https://images.pexels.com/photos/141966/pexels-photo-141966.jpeg",
        "price" : 3,
        "priceId" : "price_1Ow4fNSJIWtlP70gOmiBjvgf",
        "description" : "Best sharpener in the world. "
    },
    "Lemon": {
        "image" : "https://images.pexels.com/photos/1343537/pexels-photo-1343537.jpeg",
        "price" : 5,
        "priceId" : "price_1Ow4fBSJIWtlP70gqqnKAjRJ",
        "description" : "Fresh lemons from the farm nearby"
    },
    "Eraser": {
        "image" : "https://images.pexels.com/photos/35202/eraser-office-supplies-office-office-accessories.jpg",
        "price" : 5,
        "priceId" : "price_1Ow4ewSJIWtlP70gMpSxNVRb",
        "description" : "Rub your worries away with the most amazing eraser"
    },
}

detection_result_queue = ["Lemon","Lemon", "Eraser", "Pen"]

@app.route('/', methods=['GET'])
def index():
    return render_template("index.html", items = items_list)
#pen - v1 pen-version2
@app.route('/upload', methods=['POST'])
def upload():

    global detection_result_queue
    if 'imageFile' not in request.files:
        return "Bad request : Image not recieved", 400

    file = request.files["imageFile"].read()
    detection_result_queue = yolo_img.yolo_img(frombuffer(file, uint8))
    if "pen - v1 pen-version2" in detection_result_queue:
        tm = detection_result_queue["pen - v1 pen-version2"]
        del detection_result_queue["pen - v1 pen-version2"]
        detection_result_queue["Pen"] = tm

    print(detection_result_queue)
    return "OK", 200

def countFreq(arr):
    global detection_result_queue
    mp =  dict()
    print(arr)
    for k,v in detection_result_queue.items():
        
        mp[k] = {}
        mp[k]['quantity'] = v
        mp[k]['price'] = items_list[k]['priceId']

    return mp

@app.route('/cart')
def stream_cart():

    def event_stream():
        global detection_result_queue
        buffer = detection_result_queue
        while True:
            if buffer != detection_result_queue:
                print("BITCH LASAGNA!!!")
                buffer = detection_result_queue.copy()
                yield f"data: {json.dumps(detection_result_queue)}\n\n"
            sleep(2)

    return Response(event_stream(), content_type="text/event-stream")

@app.route('/create-checkout-session', methods=['POST'])
def create_checkout_session():
    print(detection_result_queue)
    print(countFreq(detection_result_queue).values())
    try:
        checkout_session = stripe.checkout.Session.create(
            # line_items=[
            #     {
            #         # Provide the exact Price ID (for example, pr_1234) of the product you want to sell
            #         'price': 'price_1Ow4fiSJIWtlP70gj6apCosP' ,
            #         'quantity': 1,
            #     },
            # ],
            line_items= list(countFreq(detection_result_queue).values()),
            mode='payment',
            success_url=YOUR_DOMAIN + '/success.html',
            cancel_url=YOUR_DOMAIN + '/cancel.html',
        )
    except Exception as e:
        return str(e)

    return redirect(checkout_session.url, code=303)

if __name__ == '__main__':
    app.run(debug=True,port=3000,host="0.0.0.0")
