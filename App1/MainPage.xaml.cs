using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading;
using Telegram.Controls;
using Telegram.Streams;
using Windows.ApplicationModel;
using Windows.Foundation;
using Windows.Storage;
using Windows.System;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media.Imaging;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace App1
{
    [ComImport]
    [Guid("905A0FEF-BC53-11DF-8C49-001E4FC686DA"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IBufferByteAccess
    {
        unsafe void Buffer(out byte* value);
    }

    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        public MainPage()
        {
            this.InitializeComponent();

            Loaded += MainPage_Loaded;
        }

        private async void MainPage_Loaded(object sender, RoutedEventArgs e)
        {
            var file = await Package.Current.InstalledLocation.GetFileAsync("5335006395664179964_102.tgs");
            var dest = await ApplicationData.Current.LocalFolder.CreateFileAsync("test1.tgs", CreationCollisionOption.ReplaceExisting);
            await file.CopyAndReplaceAsync(dest);
            file = await Package.Current.InstalledLocation.GetFileAsync("5335006395664179964.tgs");
            dest = await ApplicationData.Current.LocalFolder.CreateFileAsync("test2.tgs", CreationCollisionOption.ReplaceExisting);
            await file.CopyAndReplaceAsync(dest);

            var path = Path.Combine(ApplicationData.Current.LocalFolder.Path, "Files");
            if (Directory.Exists(path))
            {
                var files = Directory.GetFiles(path);

                foreach (var item in files)
                {
                    if (item.EndsWith("webp"))
                    {
                        _ciao.Add(item);
                    }
                }
            }
            else
            {
                Directory.CreateDirectory(path);

                var files = Directory.GetFiles(Path.Combine(Package.Current.InstalledLocation.Path, "Files"));

                foreach (var item in files)
                {
                    _ciao.Add(Path.Combine(path, Path.GetFileName(item)));
                    File.Copy(item, Path.Combine(path, Path.GetFileName(item)));
                }
            }
        }

        private List<string> _ciao = new();

        private int _index = 0;

        private unsafe void Button_Click(object sender, RoutedEventArgs e)
        {
            foreach (AnimatedImage2 player in Interactions.Children)
            {
                //if (player.Source.Contains("test1"))
                //{
                //    player.Source = Path.Combine(ApplicationData.Current.LocalFolder.Path, "test2.tgs");
                //}
                //else
                //{
                //    player.Source = Path.Combine(ApplicationData.Current.LocalFolder.Path, "test1.tgs");
                //}
            }


            ////try
            ////{
            ////    var file = await Package.Current.InstalledLocation.GetFileAsync($"{_stickers[index]}.tgs");
            ////    var result = await file.CopyAsync(ApplicationData.Current.LocalFolder, file.Name, NameCollisionOption.ReplaceExisting);

            ////    var cache = await ApplicationData.Current.LocalFolder.TryGetItemAsync($"{file.Name}.cache");
            ////    if (cache != null)
            ////    {
            ////        await cache.DeleteAsync();
            ////    }

            ////    //await Task.Run(() => LottieAnimation.LoadFromFile(result.Path, false, null).RenderSync(Path.Combine(ApplicationData.Current.LocalFolder.Path, "test.png"), 512, 512, 0));
            ////}
            ////catch { }

            ////return;

            //var folder = await Package.Current.InstalledLocation.GetFolderAsync("Files");
            //var files = await folder.GetFilesAsync();

            //var x = 0;
            //var y = 0;

            //Panel.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Auto) });

            //var file = await Package.Current.InstalledLocation.GetFileAsync("5226445049943826160.tgs");

            ////foreach (var file in files)
            //{

            //    var player = new LottieView
            //    {
            //        AutoPlay = true,
            //        Width = 256,
            //        Height = 256,
            //        FrameSize = new Windows.Graphics.SizeInt32 { Width = 256, Height = 256 },
            //        IsCachingEnabled = true,
            //        ColorReplacements = new Dictionary<int, int> { { 0xffffff, 0x000000 } },
            //        Source = new Uri("file:///" + file.Path)
            //    };

            //    Grid.SetColumn(player, x);
            //    Grid.SetRow(player, y);
            //    Panel.Children.Add(player);

            //    index = (index + 1) % _stickers.Length;

            //    if (y == 0)
            //    {
            //        Panel.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Auto) });
            //    }

            //    x++;

            //    if (x == 20)
            //    {
            //        Panel.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Auto) });

            //        y++;
            //        x = 0;
            //    }
            //}
        }

        private int _count;

        private void LayoutUpdated2(object sender, object e)
        {
            _count++;

            if (_count > 1000)
            {
                this.LayoutUpdated -= LayoutUpdated2;
            }
            else
            {
                Button_Click_1(null, null);
            }
        }

        [MethodImpl(MethodImplOptions.NoOptimization)]
        private async void Button2_Click(object sender, RoutedEventArgs e)
        {
            _count = 0;
            _watch = null;
            Interactions.Children.Clear();
            return;

            var bitmap = new WriteableBitmap(512, 512);
            //var buffer = Telegram.Native.BufferSurface.Create(512 * 512 * 4);

            var bytes = new byte[512 * 512 * 4];

            for (int i = 0; i < bytes.Length; i++)
            {
                bytes[i] = (byte)(i % 256);
            }

            var buffer = bytes.AsBuffer();

            var watch = Stopwatch.StartNew();

            for (int i = 0; i < 1_000; i++)
            {
                Telegram.Native.BufferSurface.Copy(buffer, bitmap.PixelBuffer);
            }

            watch.Stop();

            var pixels = bitmap.PixelBuffer.ToArray();

            if (sender is Button button)
            {
                button.Content = $"{watch.ElapsedMilliseconds} ms";
            }
        }

        private UIElement _temp;
        private Stopwatch _watch;

        private void Button_Click_1(object sender, RoutedEventArgs e)
        {
            //if (_temp != null)
            //{
            //    Interactions.Children.Add(_temp);
            //    _temp = null;
            //    return;
            //}
            //else if (Interactions.Children.Count > 0)
            //{
            //    _temp = Interactions.Children[0];
            //    Interactions.Children.Clear();
            //    return;
            //}

            var dispatcher = DispatcherQueue.GetForCurrentThread();

            var player = new AnimatedImage2();
            player.Width = 48;
            player.Height = 48;
            player.FrameSize = new Size(48, 48);
            player.DecodeFrameType = DecodePixelType.Logical;
            //player.IsFlipped = false;
            //player.IsLoopingEnabled = true;
            player.IsHitTestVisible = false;
            //player.FrameSize = new Size(270 * 2, 270 * 2);
            player.Source = new LocalFileSource(_ciao[(_index++) % _ciao.Count]); //Path.Combine(ApplicationData.Current.LocalFolder.Path, "test1.tgs");
            //player.PositionChanged += (s, args) =>
            //{
            //    return;

            //    if (args == 1)
            //    {
            //        dispatcher.TryEnqueue(() =>
            //        {
            //            Interactions.Children.Remove(player);
            //            InteractionsPopup.IsOpen = false;
            //        });
            //    }
            //};

            var left = 75;
            var right = 15;
            var top = 45;
            var bottom = 45;

            _watch ??= Stopwatch.StartNew();

            //player.Margin = new Thickness(-right, -top, -left, -bottom);
            player.Loaded += (s, args) =>
            {
                _count++;
                if (_count < 1)
                {
                    Button_Click_1(null, null);
                }
                else
                {
                    _watch.Stop();
                    Batong.Content = _watch.ElapsedMilliseconds;
                }

            };

            //player.Play();

            //Interactions.Children.Clear();
            Interactions.Children.Add(player);
            InteractionsPopup.IsOpen = true;

            //Interactions.Children.Clear();
        }

        private void Button3_Click(object sender, RoutedEventArgs e)
        {
            foreach (AnimatedImage2 player in Interactions.Children)
            {
                player.Play();
            }
        }

        private void Button4_Click(object sender, RoutedEventArgs e)
        {
            foreach (AnimatedImage2 player in Interactions.Children)
            {
                player.Pause();
            }
        }

        private void Button5_Click(object sender, RoutedEventArgs e)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
    }
}
