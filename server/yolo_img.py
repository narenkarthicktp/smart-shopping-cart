from ultralytics import YOLO
from numpy import frombuffer, uint8
import cv2

def yolo_img(img_scalar):

    model = YOLO("models/final.pt")
    img = cv2.imdecode(img_scalar, cv2.IMREAD_COLOR)
    #img = img[200:480,0:640]
    results = model.predict(source=img, save=True, save_txt=True, conf=0.4)

    obj = {}
    for r in results:
        for c in r.boxes.cls:
            key = model.names[int(c)]
            obj[key] = 1 if key not in obj else (obj[key] + 1)

    return obj

# if __name__ == '__main__':
#     with open(f"uploads/sample-product-005.jpg", 'rb') as f:
#         print(yolo_img(frombuffer(f.read(), uint8)))
