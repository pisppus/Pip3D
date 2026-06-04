using System.Text;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Pip3DEditor.ViewModels;

namespace Pip3DEditor.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
    }

    private MainWindowViewModel? ViewModel => DataContext as MainWindowViewModel;

    private async void OpenScene_Click(object? sender, RoutedEventArgs e)
    {
        if (StorageProvider is null || ViewModel is null)
        {
            return;
        }

        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Open Pip3D Scene",
            AllowMultiple = false,
            FileTypeFilter =
            [
                new FilePickerFileType("Pip3D Scene")
                {
                    Patterns = ["*.pip3dscene", "*.json"]
                }
            ]
        });

        var file = files.FirstOrDefault();
        if (file is null)
        {
            return;
        }

        await using var stream = await file.OpenReadAsync();
        using var reader = new StreamReader(stream);
        var json = await reader.ReadToEndAsync();
        ViewModel.LoadSceneJson(json, file.TryGetLocalPath());
    }

    private async void SaveScene_Click(object? sender, RoutedEventArgs e)
    {
        if (ViewModel is null)
        {
            return;
        }

        if (string.IsNullOrWhiteSpace(ViewModel.ScenePath))
        {
            await SaveSceneAsInternalAsync();
            return;
        }

        await File.WriteAllTextAsync(ViewModel.ScenePath, ViewModel.ExportSceneJson(), Encoding.UTF8);
        ViewModel.MarkSaved(ViewModel.ScenePath);
    }

    private async void SaveSceneAs_Click(object? sender, RoutedEventArgs e)
    {
        await SaveSceneAsInternalAsync();
    }

    private void AddRootAsset_Click(object? sender, RoutedEventArgs e)
    {
        ViewModel?.AddRootAssetCommand.Execute(ViewModel.SelectedAsset);
    }

    private void AddChildAsset_Click(object? sender, RoutedEventArgs e)
    {
        ViewModel?.AddChildAssetCommand.Execute(ViewModel.SelectedAsset);
    }

    private void AssetList_DoubleTapped(object? sender, RoutedEventArgs e)
    {
        ViewModel?.AddRootAssetCommand.Execute(ViewModel.SelectedAsset);
    }

    private async Task SaveSceneAsInternalAsync()
    {
        if (StorageProvider is null || ViewModel is null)
        {
            return;
        }

        var file = await StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "Save Pip3D Scene",
            SuggestedFileName = $"{ViewModel.SceneName}.pip3dscene",
            FileTypeChoices =
            [
                new FilePickerFileType("Pip3D Scene")
                {
                    Patterns = ["*.pip3dscene"]
                }
            ]
        });

        if (file is null)
        {
            return;
        }

        await using var stream = await file.OpenWriteAsync();
        await using var writer = new StreamWriter(stream, Encoding.UTF8);
        await writer.WriteAsync(ViewModel.ExportSceneJson());
        await writer.FlushAsync();
        ViewModel.MarkSaved(file.TryGetLocalPath());
    }
}
