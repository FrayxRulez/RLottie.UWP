using Microsoft.Graphics.Canvas;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

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

        private RLottie.Animation _json;
        private CanvasBitmap[] _frames;
        private int _index;

        private async void Button_Click(object sender, RoutedEventArgs e)
        {
            var watch1 = Stopwatch.StartNew();

            _json = RLottie.Animation.LoadFromFile("1258816259753938.tgs");
            _frames = new CanvasBitmap[_json.TotalFrame];

            watch1.Stop();
            var watch2 = Stopwatch.StartNew();

            //var file = await Package.Current.InstalledLocation.GetFileAsync("1258816259753938.tgs");
            //using (var stream = await DecompressAsync(file))
            //using (var reader = new StreamReader(stream))
            //{
            //    var json = reader.ReadToEnd();

            //    _json = RLottie.Animation.LoadFromData(json.Replace("\"tgs\":1,", string.Empty));
            //    _frames = new CanvasBitmap[_json.TotalFrame];
            //}

            watch2.Stop();

            var dialog = new ContentDialog();
            dialog.Content = watch1.Elapsed.ToString() + "\r\n" + watch2.Elapsed.ToString();
            dialog.PrimaryButtonText = "OK";

            await dialog.ShowAsync();

            Canvas.TargetElapsedTime = TimeSpan.FromSeconds(_json.Duration / _json.TotalFrame);
            Canvas.Paused = false;
            Canvas.Invalidate();
        }

        private async Task<System.IO.MemoryStream> DecompressAsync(StorageFile file)
        {
            System.IO.MemoryStream text;

            using (var originalFileStream = await System.IO.WindowsRuntimeStorageExtensions.OpenStreamForReadAsync(file))
            {
                var decompressedFileStream = new System.IO.MemoryStream();
                using (var decompressionStream = new GZipStream(originalFileStream, CompressionMode.Decompress))
                {
                    await decompressionStream.CopyToAsync(decompressedFileStream);
                }

                decompressedFileStream.Seek(0, System.IO.SeekOrigin.Begin);
                text = decompressedFileStream;
            }

            return text;
        }

        private void CanvasControl_Draw(Microsoft.Graphics.Canvas.UI.Xaml.ICanvasAnimatedControl sender, Microsoft.Graphics.Canvas.UI.Xaml.CanvasAnimatedDrawEventArgs args)
        {
            if (_json == null)
            {
                return;
            }

            if (_frames[_index] == null)
            {
                var bytes = _json.RenderSync(_index);
                _frames[_index] = CanvasBitmap.CreateFromBytes(sender, bytes, 512, 512, Windows.Graphics.DirectX.DirectXPixelFormat.B8G8R8A8UIntNormalized);
            }

            args.DrawingSession.DrawImage(_frames[_index], new Rect(0, 0, 256, 256));

            _index = (_index + 1) % _json.TotalFrame;
        }

        private void Button2_Click(object sender, RoutedEventArgs e)
        {
            Canvas.Paused = true;

            for (int i = 0; i < _frames.Length; i++)
            {
                if (_frames[i] != null)
                {
                    _frames[i].Dispose();
                    _frames[i] = null;
                }
            }
        }

        private void Canvas_CreateResources(Microsoft.Graphics.Canvas.UI.Xaml.CanvasAnimatedControl sender, Microsoft.Graphics.Canvas.UI.CanvasCreateResourcesEventArgs args)
        {
            if (_frames == null)
            {
                return;
            }

            for (int i = 0; i < _frames.Length; i++)
            {
                if (_frames[i] != null)
                {
                    _frames[i].Dispose();
                    _frames[i] = null;
                }
            }
        }
    }
}
