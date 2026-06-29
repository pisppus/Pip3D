namespace Pip3DEditor.ViewModels;

public sealed class AssetItemViewModel
{
    public required string Title { get; init; }
    public required string Category { get; init; }
    public required string AssetKey { get; init; }
    public required string NodeType { get; init; }
    public required string ColorHex { get; init; }
    public required string Summary { get; init; }
}
