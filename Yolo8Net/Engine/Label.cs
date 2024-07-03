namespace Yolov8Net
{
    public class Label
    {
        public int Id { get; init; }
        public string? Name { get; init; }
        public LabelKind Kind { get; init; } = LabelKind.Generic;

        //public Label(int id)
        //{
        //    Id = id;
        //    Name = id.ToString();
        //    Kind = LabelKind.Generic;
        //}
    }
}
