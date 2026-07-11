import os
import urllib.request
from ultralytics import YOLO

def main():
    models_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "models")
    os.makedirs(models_dir, exist_ok=True)
    
    pt_path = os.path.join(models_dir, "hand_yolov8n.pt")
    onnx_path = os.path.join(models_dir, "hand_yolov8n.onnx")
    
    if not os.path.exists(pt_path):
        print("Downloading hand_yolov8n.pt from Hugging Face...")
        url = "https://huggingface.co/Bingsu/adetailer/resolve/main/hand_yolov8n.pt"
        urllib.request.urlretrieve(url, pt_path)
        print("Download complete.")
        
    print("Exporting model to ONNX (imgsz=320)...")
    model = YOLO(pt_path)
    # Export to ONNX with dynamic=False and imgsz=320 for speed and compatibility
    model.export(format="onnx", imgsz=320, opset=12)
    
    # Ultralytics exports to hand_yolov8n.onnx in the same folder as the pt file
    result_onnx = os.path.join(models_dir, "hand_yolov8n.onnx")
    if os.path.exists(result_onnx):
        print(f"Export successful: {result_onnx}")
    else:
        print("Export failed: ONNX file not found.")

if __name__ == "__main__":
    main()
