from ultralytics import YOLO
import torch
import onnx
import onnxruntime


# Function to add metadata to the ONNX model
def add_metadata_to_onnx(model_path, labels):
    # Load the ONNX model
    model = onnx.load(model_path)

    # Add labels as metadata
    meta = model.metadata_props.add()
    meta.key = "labels"
    meta.value = ",".join(labels)  # Store labels as a comma-separated string

    # Save the updated model
    onnx.save(model, model_path)

modelname = "yolov8n"
model = torch.load(f"{modelname}.pt")  # load a pretrained model
dummy_input = torch.randn(1, 3, 640, 640)  # Example for 640x640 input size
# labels = model.names;
# with open(f"{modelname}.txt", '+w') as file:
#     for a in labels:
#        #file.write('%d: %s\n' % (a, labels[a]))
#        file.write('%s\n' % (labels[a]))
torch.onnx.export(model, dummy_input, 'model.onnx', export_params=True, opset_version=17)
# path = model.export(format="onnx")  # export the model to ONNX format

## Load a model
# model = torch.load('yolov8n.pt')

## Dummy input for the model (adjust the size according to your model's requirements)
# dummy_input = torch.randn(1, 3, 640, 640)  # Example for 640x640 input size

## Export the model to ONNX
# onnx_model_path = 'yolov8n.onnx'
# torch.onnx.export(model, export_params=True)

## List of labels
## Add labels as metadata to the ONNX model
# add_metadata_to_onnx(onnx_model_path, labels)

# Use the model

# results = model("https://ultralytics.com/images/bus.jpg")  # predict on an image

# names = model.names
# print(names)