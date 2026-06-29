using System.Collections.ObjectModel;

using CommunityToolkit.Mvvm.ComponentModel;

namespace Pip3DEditor.ViewModels;

public partial class SceneNodeViewModel : ViewModelBase
{
    private readonly MainWindowViewModel _owner;

    [ObservableProperty] private Guid _id = Guid.NewGuid();
    [ObservableProperty] private string _name;
    [ObservableProperty] private string _nodeType;
    [ObservableProperty] private string _assetKey;
    [ObservableProperty] private string _colorHex;
    [ObservableProperty] private bool _isVisible = true;
    [ObservableProperty] private bool _castsShadows = true;
    [ObservableProperty] private double _positionX;
    [ObservableProperty] private double _positionY;
    [ObservableProperty] private double _positionZ;
    [ObservableProperty] private double _rotationX;
    [ObservableProperty] private double _rotationY;
    [ObservableProperty] private double _rotationZ;
    [ObservableProperty] private double _scaleX = 1.0;
    [ObservableProperty] private double _scaleY = 1.0;
    [ObservableProperty] private double _scaleZ = 1.0;
    [ObservableProperty] private bool _isExpanded = true;

    public ObservableCollection<SceneNodeViewModel> Children { get; } = [];

    public SceneNodeViewModel? Parent { get; private set; }

    public SceneNodeViewModel(MainWindowViewModel owner, string name, string nodeType, string assetKey, string colorHex)
    {
        _owner = owner;
        _name = name;
        _nodeType = nodeType;
        _assetKey = assetKey;
        _colorHex = colorHex;
    }

    public string TransformSummary =>
        $"P ({PositionX:0.0}, {PositionY:0.0}, {PositionZ:0.0})  R ({RotationX:0.0}, {RotationY:0.0}, {RotationZ:0.0})  S ({ScaleX:0.0}, {ScaleY:0.0}, {ScaleZ:0.0})";

    public double RadiusEstimate =>
        Math.Max(0.35, Math.Max(Math.Abs(ScaleX), Math.Max(Math.Abs(ScaleY), Math.Abs(ScaleZ)))) * 14.0;

    public void AttachTo(SceneNodeViewModel? parent)
    {
        Parent = parent;
    }

    public SceneNodeDto ToDto() =>
        new()
        {
            Id = Id,
            Name = Name,
            NodeType = NodeType,
            AssetKey = AssetKey,
            ColorHex = ColorHex,
            IsVisible = IsVisible,
            CastsShadows = CastsShadows,
            PositionX = PositionX,
            PositionY = PositionY,
            PositionZ = PositionZ,
            RotationX = RotationX,
            RotationY = RotationY,
            RotationZ = RotationZ,
            ScaleX = ScaleX,
            ScaleY = ScaleY,
            ScaleZ = ScaleZ,
            Children = [.. Children.Select(static c => c.ToDto())]
        };

    public static SceneNodeViewModel FromDto(MainWindowViewModel owner, SceneNodeDto dto)
    {
        var node = new SceneNodeViewModel(owner, dto.Name, dto.NodeType, dto.AssetKey, dto.ColorHex)
        {
            Id = dto.Id == Guid.Empty ? Guid.NewGuid() : dto.Id,
            IsVisible = dto.IsVisible,
            CastsShadows = dto.CastsShadows,
            PositionX = dto.PositionX,
            PositionY = dto.PositionY,
            PositionZ = dto.PositionZ,
            RotationX = dto.RotationX,
            RotationY = dto.RotationY,
            RotationZ = dto.RotationZ,
            ScaleX = dto.ScaleX,
            ScaleY = dto.ScaleY,
            ScaleZ = dto.ScaleZ
        };

        foreach (var childDto in dto.Children)
        {
            var child = FromDto(owner, childDto);
            child.AttachTo(node);
            node.Children.Add(child);
        }

        return node;
    }

    partial void OnNameChanged(string value) => NotifyChanged();
    partial void OnNodeTypeChanged(string value) => NotifyChanged();
    partial void OnAssetKeyChanged(string value) => NotifyChanged();
    partial void OnColorHexChanged(string value) => NotifyChanged();
    partial void OnIsVisibleChanged(bool value) => NotifyChanged();
    partial void OnCastsShadowsChanged(bool value) => NotifyChanged();
    partial void OnPositionXChanged(double value) => NotifyChanged();
    partial void OnPositionYChanged(double value) => NotifyChanged();
    partial void OnPositionZChanged(double value) => NotifyChanged();
    partial void OnRotationXChanged(double value) => NotifyChanged();
    partial void OnRotationYChanged(double value) => NotifyChanged();
    partial void OnRotationZChanged(double value) => NotifyChanged();
    partial void OnScaleXChanged(double value) => NotifyChanged();
    partial void OnScaleYChanged(double value) => NotifyChanged();
    partial void OnScaleZChanged(double value) => NotifyChanged();

    private void NotifyChanged()
    {
        OnPropertyChanged(nameof(TransformSummary));
        OnPropertyChanged(nameof(RadiusEstimate));
        _owner.NotifySceneChanged();
    }
}

public sealed class SceneNodeDto
{
    public Guid Id { get; set; }
    public string Name { get; set; } = "Node";
    public string NodeType { get; set; } = "Mesh";
    public string AssetKey { get; set; } = "Cube";
    public string ColorHex { get; set; } = "#D97A44";
    public bool IsVisible { get; set; } = true;
    public bool CastsShadows { get; set; } = true;
    public double PositionX { get; set; }
    public double PositionY { get; set; }
    public double PositionZ { get; set; }
    public double RotationX { get; set; }
    public double RotationY { get; set; }
    public double RotationZ { get; set; }
    public double ScaleX { get; set; } = 1.0;
    public double ScaleY { get; set; } = 1.0;
    public double ScaleZ { get; set; } = 1.0;
    public List<SceneNodeDto> Children { get; set; } = [];
}
