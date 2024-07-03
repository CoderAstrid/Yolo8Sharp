using SixLabors.Fonts;
using SixLabors.ImageSharp.Drawing.Processing;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Processing;
using SixLabors.ImageSharp.PixelFormats;
//using System.Drawing;
using Yolov8Net;

namespace Yolo8Net
{
    public partial class FrmMain : Form
    {
        public FrmMain()
        {
            InitializeComponent();
        }

        private void DrawBoxes(int modelInputHeight, int modelInputWidth, SixLabors.ImageSharp.Image image, Prediction[] predictions)
        {
            foreach (var pred in predictions)
            {
                var originalImageHeight = image.Height;
                var originalImageWidth = image.Width;

                var x = (int)Math.Max(pred.Rectangle.X, 0);
                var y = (int)Math.Max(pred.Rectangle.Y, 0);
                var width = (int)Math.Min(originalImageWidth - x, pred.Rectangle.Width);
                var height = (int)Math.Min(originalImageHeight - y, pred.Rectangle.Height);

                //Note that the output is already scaled to the original image height and width.

                // Bounding Box Text
                string text = $"{pred.Label.Name} [{pred.Score}]";
                //var size = TextMeasurer.MeasureSize(text, new TextOptions(font));

                image.Mutate(d => d.Draw(SixLabors.ImageSharp.Drawing.Processing.Pens.Solid(SixLabors.ImageSharp.Color.Yellow, 2), new SixLabors.ImageSharp.Rectangle(x, y, width, height)));
                //image.Mutate(d => d.DrawPolygon(Color.Yellow, 2, new PointF(x, y), new PointF(x + width, y), new PointF(x + width, y + height), new PointF(x, y + height)));

                //image.Mutate(d => d.DrawText(text, font, Color.Yellow, new Point(x, (int)(y - size.Height - 1))));

            }
        }
        private void btnRun_Click(object sender, EventArgs e)
        {
            string modelpath = Path.Combine(Application.StartupPath, "model/yolov8n.onnx");
            using var yolo = YoloV8Predictor.Create(modelpath);

                var fileName = txtPath.Text;
                using var image = SixLabors.ImageSharp.Image.Load(fileName);
                var predictions = yolo.Predict(image);

                DrawBoxes(yolo.ModelInputHeight, yolo.ModelInputWidth, image, predictions);

                image.SaveAsync("D:/1.jpg");
            /*
            Detection detection = new Detection(Application.StartupPath);
            if (detection.Process(txtPath.Text))
            {
                var res = detection.res;
                //txtResult.Text = $"Class: {res.ClassId}, Confidence: {res.Confidence}, Box: {res.Box}";
            }
            */
        }

        private void btnPrev_Click(object sender, EventArgs e)
        {

        }

        private void btnNext_Click(object sender, EventArgs e)
        {

        }

        private void btnLoad_Click(object sender, EventArgs e)
        {
            // load images and videos
            var OFD = new OpenFileDialog();
            OFD.Title = "Open";
            OFD.Filter =
                "Images (*.bmp; *.emf; *.exif; *.gif; *.ico; *.jpg; *.png; *.tiff; *.wmf)|*.bmp; *.emf; *.exif; *.gif; *.ico; *.jpg; *.png; *.tiff; *.wmf|" +
                "Videos(*.avi;*.mp4)|*.avi;*.mp4|" +
                "All files|*.*";
            if (OFD.ShowDialog() == DialogResult.OK)
            {
                var filename = OFD.FileName;
                this.Text = $"DeOldify Dotnet: {filename}";
                OpenImage(OFD.FileName);
            }
        }

        private void chkAuto_CheckedChanged(object sender, EventArgs e)
        {

        }

        private void OpenImage(string filename)
        {
            var img = System.Drawing.Image.FromFile(filename);
            picImage.Image = img;
            txtPath.Text = filename;
        }
    }
}
