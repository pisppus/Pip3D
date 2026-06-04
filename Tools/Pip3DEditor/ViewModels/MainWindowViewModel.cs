using System.Collections.ObjectModel;
using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace Pip3DEditor.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    private readonly string _liveScenePath = ResolveLiveScenePath();
    private readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public ObservableCollection<SceneNodeViewModel> RootNodes { get; } = [];
    public ObservableCollection<AssetItemViewModel> Assets { get; } = [];

    [ObservableProperty] private string _sceneName = "Untitled Scene";
    [ObservableProperty] private string _scenePath = string.Empty;
    [ObservableProperty] private string _statusText = "Ready";
    [ObservableProperty] private SceneNodeViewModel? _selectedNode;
    [ObservableProperty] private AssetItemViewModel? _selectedAsset;
    [ObservableProperty] private bool _isDirty;

    public MainWindowViewModel()
    {
        SeedAssets();
        CreateDefaultScene();
    }

    public void NotifySceneChanged()
    {
        IsDirty = true;
        StatusText = $"Scene changed at {DateTime.Now:HH:mm:ss}";
        OnPropertyChanged(nameof(SceneSummary));
        WriteRuntimeSnapshot();
    }

    public string SceneSummary =>
        $"{CountNodes(RootNodes)} nodes, {RootNodes.Count} root objects, {(string.IsNullOrWhiteSpace(ScenePath) ? "not saved" : Path.GetFileName(ScenePath))}";

    [RelayCommand]
    private void NewScene() => CreateDefaultScene();

    [RelayCommand]
    private void AddRootAsset(AssetItemViewModel? asset)
    {
        if (asset is null) return;
        var node = CreateNode(asset);
        RootNodes.Add(node);
        SelectedNode = node;
        NotifySceneChanged();
    }

    [RelayCommand]
    private void AddChildAsset(AssetItemViewModel? asset)
    {
        if (asset is null || SelectedNode is null) return;
        var node = CreateNode(asset);
        node.AttachTo(SelectedNode);
        SelectedNode.Children.Add(node);
        SelectedNode.IsExpanded = true;
        SelectedNode = node;
        NotifySceneChanged();
    }

    [RelayCommand]
    private void DuplicateSelected()
    {
        if (SelectedNode is null) return;
        var clone = SceneNodeViewModel.FromDto(this, SelectedNode.ToDto());
        clone.Id = Guid.NewGuid();
        clone.Name = $"{SelectedNode.Name} Copy";
        if (SelectedNode.Parent is null)
            RootNodes.Add(clone);
        else
        {
            clone.AttachTo(SelectedNode.Parent);
            SelectedNode.Parent.Children.Add(clone);
        }
        SelectedNode = clone;
        NotifySceneChanged();
    }

    [RelayCommand]
    private void DeleteSelected()
    {
        if (SelectedNode is null) return;
        if (SelectedNode.Parent is null)
            RootNodes.Remove(SelectedNode);
        else
            SelectedNode.Parent.Children.Remove(SelectedNode);
        SelectedNode = RootNodes.FirstOrDefault();
        NotifySceneChanged();
    }

    public string ExportSceneJson()
    {
        var document = new SceneDocumentDto
        {
            Version = 1,
            Name = SceneName,
            CreatedUtc = DateTime.UtcNow,
            Nodes = [.. RootNodes.Select(static n => n.ToDto())]
        };
        return JsonSerializer.Serialize(document, _jsonOptions);
    }

    public void LoadSceneJson(string json, string? path)
    {
        var scene = JsonSerializer.Deserialize<SceneDocumentDto>(json, _jsonOptions) ?? new SceneDocumentDto();
        RootNodes.Clear();
        foreach (var dto in scene.Nodes)
            RootNodes.Add(SceneNodeViewModel.FromDto(this, dto));
        SceneName = string.IsNullOrWhiteSpace(scene.Name) ? "Imported Scene" : scene.Name;
        ScenePath = path ?? string.Empty;
        SelectedNode = RootNodes.FirstOrDefault();
        IsDirty = false;
        StatusText = $"Loaded {(path is null ? "scene" : Path.GetFileName(path))}";
        OnPropertyChanged(nameof(SceneSummary));
        WriteRuntimeSnapshot();
    }

    public void MarkSaved(string? path)
    {
        ScenePath = path ?? ScenePath;
        IsDirty = false;
        StatusText = $"Saved {Path.GetFileName(ScenePath)}";
        OnPropertyChanged(nameof(SceneSummary));
        WriteRuntimeSnapshot();
    }

    private void CreateDefaultScene()
    {
        RootNodes.Clear();
        var camera = CreateNode(Assets.First(static a => a.AssetKey == "Camera"));
        camera.PositionX = -4;
        camera.PositionY = 5;
        camera.PositionZ = -6;
        var light = CreateNode(Assets.First(static a => a.AssetKey == "DirectionalLight"));
        light.PositionY = 6;
        var cube = CreateNode(Assets.First(static a => a.AssetKey == "Cube"));
        cube.PositionX = -1.5;
        cube.ScaleY = 1.2;
        var sphere = CreateNode(Assets.First(static a => a.AssetKey == "Sphere"));
        sphere.PositionX = 2.0;
        sphere.ColorHex = "#6EA8FF";
        RootNodes.Add(camera);
        RootNodes.Add(light);
        RootNodes.Add(cube);
        RootNodes.Add(sphere);
        SceneName = "Sample Scene";
        ScenePath = string.Empty;
        SelectedNode = cube;
        IsDirty = false;
        StatusText = "New sample scene";
        OnPropertyChanged(nameof(SceneSummary));
        WriteRuntimeSnapshot();
    }

    private void WriteRuntimeSnapshot()
    {
        try
        {
            var directory = Path.GetDirectoryName(_liveScenePath);
            if (!string.IsNullOrWhiteSpace(directory))
            {
                Directory.CreateDirectory(directory);
            }

            var builder = new StringBuilder();
            builder.Append("SCENE|").AppendLine(SceneName);

            var flattened = new List<RuntimeSnapshotNode>();
            foreach (var node in RootNodes)
            {
                FlattenRuntimeNode(node, RuntimeTransform.Identity, flattened);
            }

            RuntimeSnapshotNode? camera = flattened.FirstOrDefault(static node => string.Equals(node.AssetKey, "Camera", StringComparison.OrdinalIgnoreCase));
            if (camera.HasValue)
            {
                builder.Append("CAMERA|")
                    .Append(Format(camera.Value.PositionX)).Append('|')
                    .Append(Format(camera.Value.PositionY)).Append('|')
                    .Append(Format(camera.Value.PositionZ)).Append('|')
                    .Append(Format(camera.Value.RotationX)).Append('|')
                    .Append(Format(camera.Value.RotationY)).Append('|')
                    .Append(Format(camera.Value.RotationZ)).Append('|')
                    .Append("60")
                    .AppendLine();
            }

            RuntimeSnapshotNode? light = flattened.FirstOrDefault(static node => string.Equals(node.AssetKey, "DirectionalLight", StringComparison.OrdinalIgnoreCase));
            if (light.HasValue)
            {
                builder.Append("LIGHT|")
                    .Append(Format(light.Value.PositionX)).Append('|')
                    .Append(Format(light.Value.PositionY)).Append('|')
                    .Append(Format(light.Value.PositionZ)).Append('|')
                    .Append(Format(light.Value.RotationX)).Append('|')
                    .Append(Format(light.Value.RotationY)).Append('|')
                    .Append(Format(light.Value.RotationZ)).Append('|')
                    .Append("1.2|")
                    .Append(light.Value.ColorHex)
                    .AppendLine();
            }

            foreach (var node in flattened)
            {
                if (string.Equals(node.AssetKey, "Camera", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(node.AssetKey, "DirectionalLight", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(node.AssetKey, "Empty", StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }

                builder.Append("NODE|")
                    .Append(node.AssetKey).Append('|')
                    .Append(node.Name.Replace("|", "/")).Append('|')
                    .Append(node.ColorHex).Append('|')
                    .Append(node.IsVisible ? '1' : '0').Append('|')
                    .Append(node.CastsShadows ? '1' : '0').Append('|')
                    .Append(Format(node.PositionX)).Append('|')
                    .Append(Format(node.PositionY)).Append('|')
                    .Append(Format(node.PositionZ)).Append('|')
                    .Append(Format(node.RotationX)).Append('|')
                    .Append(Format(node.RotationY)).Append('|')
                    .Append(Format(node.RotationZ)).Append('|')
                    .Append(Format(node.ScaleX)).Append('|')
                    .Append(Format(node.ScaleY)).Append('|')
                    .Append(Format(node.ScaleZ))
                    .AppendLine();
            }

            File.WriteAllText(_liveScenePath, builder.ToString(), Encoding.UTF8);
        }
        catch (Exception ex)
        {
            StatusText = $"Live sync failed: {ex.Message}";
        }
    }

    private static void FlattenRuntimeNode(SceneNodeViewModel node, RuntimeTransform parent, ICollection<RuntimeSnapshotNode> output)
    {
        var current = new RuntimeTransform(
            parent.PositionX + node.PositionX,
            parent.PositionY + node.PositionY,
            parent.PositionZ + node.PositionZ,
            parent.RotationX + node.RotationX,
            parent.RotationY + node.RotationY,
            parent.RotationZ + node.RotationZ,
            parent.ScaleX * node.ScaleX,
            parent.ScaleY * node.ScaleY,
            parent.ScaleZ * node.ScaleZ);

        output.Add(new RuntimeSnapshotNode(
            node.Name,
            node.AssetKey,
            node.ColorHex,
            node.IsVisible,
            node.CastsShadows,
            current.PositionX,
            current.PositionY,
            current.PositionZ,
            current.RotationX,
            current.RotationY,
            current.RotationZ,
            current.ScaleX,
            current.ScaleY,
            current.ScaleZ));

        foreach (var child in node.Children)
        {
            FlattenRuntimeNode(child, current, output);
        }
    }

    private static string ResolveLiveScenePath()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);
        while (current is not null)
        {
            if (File.Exists(Path.Combine(current.FullName, "platformio.ini")))
            {
                return Path.Combine(current.FullName, "tools", "Pip3DEditor", "live_scene.snapshot");
            }

            current = current.Parent;
        }

        return Path.Combine(AppContext.BaseDirectory, "live_scene.snapshot");
    }

    private static string Format(double value) => value.ToString("0.###", CultureInfo.InvariantCulture);

    private SceneNodeViewModel CreateNode(AssetItemViewModel asset)
    {
        var suffix = CountNodes(RootNodes) + 1;
        return new SceneNodeViewModel(this, $"{asset.Title} {suffix}", asset.NodeType, asset.AssetKey, asset.ColorHex);
    }

    private void SeedAssets()
    {
        Assets.Clear();
        Assets.Add(new AssetItemViewModel { Title = "Cube", Category = "Primitives", AssetKey = "Cube", NodeType = "Mesh", ColorHex = "#D97A44", Summary = "Basic solid block for level layout and props." });
        Assets.Add(new AssetItemViewModel { Title = "Sphere", Category = "Primitives", AssetKey = "Sphere", NodeType = "Mesh", ColorHex = "#6EA8FF", Summary = "Rounded primitive for pickups, markers and stylized props." });
        Assets.Add(new AssetItemViewModel { Title = "Cylinder", Category = "Primitives", AssetKey = "Cylinder", NodeType = "Mesh", ColorHex = "#90B85B", Summary = "Useful for pillars, wheels and barrels." });
        Assets.Add(new AssetItemViewModel { Title = "Plane", Category = "Primitives", AssetKey = "Plane", NodeType = "Mesh", ColorHex = "#8C8C8C", Summary = "Flat surface for floors, water and terrain chunks." });
        Assets.Add(new AssetItemViewModel { Title = "Camera", Category = "Scene", AssetKey = "Camera", NodeType = "Camera", ColorHex = "#F6E27C", Summary = "Main render camera for play mode and framing." });
        Assets.Add(new AssetItemViewModel { Title = "Directional Light", Category = "Scene", AssetKey = "DirectionalLight", NodeType = "Light", ColorHex = "#FFE1A8", Summary = "Sun or moon style global light." });
        Assets.Add(new AssetItemViewModel { Title = "Empty", Category = "Helpers", AssetKey = "Empty", NodeType = "Empty", ColorHex = "#C2C9D1", Summary = "Grouping node without visible geometry." });
        SelectedAsset = Assets.FirstOrDefault();
    }

    private static int CountNodes(IEnumerable<SceneNodeViewModel> nodes)
    {
        var total = 0;
        foreach (var node in nodes)
        {
            total++;
            total += CountNodes(node.Children);
        }
        return total;
    }
}

public sealed class SceneDocumentDto
{
    public int Version { get; set; }
    public string Name { get; set; } = "Untitled Scene";
    public DateTime CreatedUtc { get; set; } = DateTime.UtcNow;
    public List<SceneNodeDto> Nodes { get; set; } = [];
}

internal readonly record struct RuntimeTransform(
    double PositionX,
    double PositionY,
    double PositionZ,
    double RotationX,
    double RotationY,
    double RotationZ,
    double ScaleX,
    double ScaleY,
    double ScaleZ)
{
    public static RuntimeTransform Identity => new(0, 0, 0, 0, 0, 0, 1, 1, 1);
}

internal readonly record struct RuntimeSnapshotNode(
    string Name,
    string AssetKey,
    string ColorHex,
    bool IsVisible,
    bool CastsShadows,
    double PositionX,
    double PositionY,
    double PositionZ,
    double RotationX,
    double RotationY,
    double RotationZ,
    double ScaleX,
    double ScaleY,
    double ScaleZ);
