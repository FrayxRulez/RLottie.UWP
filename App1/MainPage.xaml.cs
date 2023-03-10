using RLottie;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Unigram.Controls;
using Windows.ApplicationModel;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace App1
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        public MainPage()
        {
            this.InitializeComponent();
        }

        private int index = 0;
        private string[] _stickers = new string[]
        {
            "12588162597539382",
            "WalletIntroLoading2",
            "20264845623217360222"
        };

        private async void Button_Click(object sender, RoutedEventArgs e)
        {
            //try
            //{
            //    var file = await Package.Current.InstalledLocation.GetFileAsync($"{_stickers[index]}.tgs");
            //    var result = await file.CopyAsync(ApplicationData.Current.LocalFolder, file.Name, NameCollisionOption.ReplaceExisting);

            //    var cache = await ApplicationData.Current.LocalFolder.TryGetItemAsync($"{file.Name}.cache");
            //    if (cache != null)
            //    {
            //        await cache.DeleteAsync();
            //    }

            //    //await Task.Run(() => LottieAnimation.LoadFromFile(result.Path, false, null).RenderSync(Path.Combine(ApplicationData.Current.LocalFolder.Path, "test.png"), 512, 512, 0));
            //}
            //catch { }

            //return;

            var folder = await Package.Current.InstalledLocation.GetFolderAsync("Files");
            var files = await folder.GetFilesAsync();

            var x = 0;
            var y = 0;

            Panel.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Auto) });

            var file = await Package.Current.InstalledLocation.GetFileAsync("5226445049943826160.tgs");

            //foreach (var file in files)
            {

                var player = new LottieView
                {
                    AutoPlay = true,
                    Width = 256,
                    Height = 256,
                    FrameSize = new Windows.Graphics.SizeInt32 { Width = 256, Height = 256 },
                    IsCachingEnabled = true,
                    ColorReplacements = new Dictionary<int, int> { { 0xffffff, 0x000000 } },
                    Source = new Uri("file:///" + file.Path)
                };

                Grid.SetColumn(player, x);
                Grid.SetRow(player, y);
                Panel.Children.Add(player);

                index = (index + 1) % _stickers.Length;

                if (y == 0)
                {
                    Panel.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Auto) });
                }

                x++;

                if (x == 20)
                {
                    Panel.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Auto) });

                    y++;
                    x = 0;
                }
            }
        }

        private void Button2_Click(object sender, RoutedEventArgs e)
        {
            if (Panel.Children.Count > 0)
            {
                Panel.Children.RemoveAt(0);
            }

            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
    }
}
