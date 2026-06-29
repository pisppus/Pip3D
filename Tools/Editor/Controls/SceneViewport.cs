using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Pip3DEditor.ViewModels;

namespace Pip3DEditor.Controls;

public sealed class SceneViewport : Control
{
    public static readonly StyledProperty<IEnumerable<SceneNodeViewModel>?> NodesProperty =
        AvaloniaProperty.Register<SceneViewport, IEnumerable<SceneNodeViewModel>?>(nameof(Nodes));

    public static readonly StyledProperty<SceneNodeViewModel?> SelectedNodeProperty =
        AvaloniaProperty.Register<SceneViewport, SceneNodeViewModel?>(nameof(SelectedNode), defaultBindingMode: Avalonia.Data.BindingMode.TwoWay);

    private readonly List<ProjectedNode> _projectedNodes = [];

    static SceneViewport()
    {
        AffectsRender<SceneViewport>(NodesProperty, SelectedNodeProperty);
    }

    public IEnumerable<SceneNodeViewModel>? Nodes
    {
        get => GetValue(NodesProperty);
        set => SetValue(NodesProperty, value);
    }

    public SceneNodeViewModel? SelectedNode
    {
        get => GetValue(SelectedNodeProperty);
        set => SetValue(SelectedNodeProperty, value);
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);

        var bounds = Bounds;
        if (bounds.Width <= 1 || bounds.Height <= 1)
        {
            return;
        }

        var skyBrush = new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
            GradientStops =
            [
                new GradientStop(Color.Parse("#22365E"), 0.0),
                new GradientStop(Color.Parse("#3B6498"), 0.58),
                new GradientStop(Color.Parse("#C9D6D8"), 1.0)
            ]
        };
        context.FillRectangle(skyBrush, bounds);

        var horizonY = bounds.Height * 0.64;
        context.FillRectangle(new SolidColorBrush(Color.Parse("#B4C8CC")), new Rect(0, horizonY, bounds.Width, bounds.Height - horizonY));

        var gridPen = new Pen(new SolidColorBrush(Color.Parse("#3A526C")), 1);
        for (var x = 0.0; x < bounds.Width; x += 48.0)
        {
            context.DrawLine(gridPen, new Point(x, horizonY), new Point(bounds.Width * 0.5 + (x - bounds.Width * 0.5) * 0.2, bounds.Height));
        }
        for (var y = horizonY; y < bounds.Height; y += 26.0)
        {
            context.DrawLine(gridPen, new Point(0, y), new Point(bounds.Width, y));
        }

        _projectedNodes.Clear();
        if (Nodes is null)
        {
            return;
        }

        foreach (var node in Flatten(Nodes))
        {
            if (!node.IsVisible)
            {
                continue;
            }

            var px = bounds.Width * 0.5 + node.PositionX * 54.0;
            var py = horizonY - node.PositionZ * 26.0 - node.PositionY * 44.0;
            var radius = Math.Clamp(node.RadiusEstimate * 0.45, 8.0, 34.0);
            _projectedNodes.Add(new ProjectedNode(node, new Point(px, py), radius));
        }

        foreach (var item in _projectedNodes.OrderBy(static item => item.Node.PositionZ))
        {
            var fill = new SolidColorBrush(ParseColor(item.Node.ColorHex, "#7A869A"));
            var border = new Pen(item.Node == SelectedNode ? Brushes.White : Brushes.Black, item.Node == SelectedNode ? 3 : 1);

            if (item.Node.AssetKey == "Plane")
            {
                var rect = new Rect(item.Center.X - 80, item.Center.Y - 10, 160, 20);
                context.FillRectangle(fill, rect);
                context.DrawRectangle(border, rect);
            }
            else
            {
                context.DrawEllipse(fill, border, item.Center, item.Radius, item.Radius);
            }

            var glyph = new FormattedText(
                item.Node.Name,
                System.Globalization.CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight,
                new Typeface("Segoe UI"),
                12,
                Brushes.White);
            context.DrawText(glyph, new Point(item.Center.X + item.Radius + 6, item.Center.Y - 8));
        }

        var hud = new FormattedText(
            "Editor Viewport  |  Preview  |  Click a node to inspect",
            System.Globalization.CultureInfo.InvariantCulture,
            FlowDirection.LeftToRight,
            new Typeface("Segoe UI"),
            12,
            new SolidColorBrush(Color.Parse("#F2F5F8")));
        context.DrawText(hud, new Point(16, 14));
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);

        var point = e.GetPosition(this);
        ProjectedNode? hit = null;

        foreach (var item in _projectedNodes)
        {
            var dx = point.X - item.Center.X;
            var dy = point.Y - item.Center.Y;
            var distanceSquared = dx * dx + dy * dy;
            if (distanceSquared > item.Radius * item.Radius * 1.2)
            {
                continue;
            }

            if (hit is null || distanceSquared < hit.DistanceSquared)
            {
                item.DistanceSquared = distanceSquared;
                hit = item;
            }
        }

        if (hit is not null)
        {
            SelectedNode = hit.Node;
            InvalidateVisual();
        }
    }

    private static IEnumerable<SceneNodeViewModel> Flatten(IEnumerable<SceneNodeViewModel> nodes)
    {
        foreach (var node in nodes)
        {
            yield return node;
            foreach (var child in Flatten(node.Children))
            {
                yield return child;
            }
        }
    }

    private static Color ParseColor(string colorHex, string fallback)
    {
        try
        {
            return Color.Parse(colorHex);
        }
        catch
        {
            return Color.Parse(fallback);
        }
    }

    private sealed class ProjectedNode
    {
        public ProjectedNode(SceneNodeViewModel node, Point center, double radius)
        {
            Node = node;
            Center = center;
            Radius = radius;
        }

        public SceneNodeViewModel Node { get; }
        public Point Center { get; }
        public double Radius { get; }
        public double DistanceSquared { get; set; }
    }
}
