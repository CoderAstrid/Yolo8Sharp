namespace Yolov8Net
{
    public class Prediction
    {
        public Label? Label { get; init; }
        public SixLabors.ImageSharp.RectangleF Rectangle { get; init; }
        public float Score { get; init; }
    }
}
