﻿using RLottie;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using Telegram.Common;
using Telegram.Native;
using Telegram.Streams;
using Windows.Foundation;
using Windows.Graphics;
using Windows.Storage.Streams;
using Windows.System;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;

namespace Telegram.Controls
{
    public class AnimatedImage2 : Control, IPlayerView
    {
        enum PlayingState
        {
            None,
            Playing,
            Paused
        }

        private bool _templateApplied;
        private int _loaded;

        private PlayingState _state;
        private bool _delayedPlay;

        private double _rasterizationScale;

        private AnimatedImagePresenter _presenter;

        public AnimatedImage2()
        {
            DefaultStyleKey = typeof(AnimatedImage2);

            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            //Logger.Debug();

            _loaded++;
            Prepare();

            XamlRoot.Changed -= OnRasterizationScaleChanged;
            XamlRoot.Changed += OnRasterizationScaleChanged;
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            //Logger.Debug();

            _loaded--;
            Unload();

            XamlRoot.Changed -= OnRasterizationScaleChanged;
        }

        public void Play()
        {
            if (_presenter != null)
            {
                _delayedPlay = false;

                if (_state != PlayingState.Playing)
                {
                    _state = PlayingState.Playing;
                    _presenter.Play();
                }
            }
            else
            {
                _delayedPlay = true;
            }
        }

        public void Pause()
        {
            _delayedPlay = false;

            if (_presenter != null)
            {
                if (_state == PlayingState.Playing)
                {
                    _state = PlayingState.Paused;
                    _presenter.Pause();
                }
            }
        }

        #region Source

        public AnimatedImageSource Source
        {
            get { return (AnimatedImageSource)GetValue(SourceProperty); }
            set { SetValue(SourceProperty, value); }
        }

        public static readonly DependencyProperty SourceProperty =
            DependencyProperty.Register("Source", typeof(AnimatedImageSource), typeof(AnimatedImage2), new PropertyMetadata(null, OnPropertyChanged));

        #endregion

        #region LoopCount

        public int LoopCount
        {
            get { return (int)GetValue(LoopCountProperty); }
            set { SetValue(LoopCountProperty, value); }
        }

        public static readonly DependencyProperty LoopCountProperty =
            DependencyProperty.Register("LoopCount", typeof(int), typeof(AnimatedImage2), new PropertyMetadata(0, OnPropertyChanged));

        #endregion

        #region AutoPlay

        public bool AutoPlay
        {
            get { return (bool)GetValue(AutoPlayProperty); }
            set { SetValue(AutoPlayProperty, value); }
        }

        public static readonly DependencyProperty AutoPlayProperty =
            DependencyProperty.Register("AutoPlay", typeof(bool), typeof(AnimatedImage2), new PropertyMetadata(false, OnPropertyChanged));

        #endregion

        #region IsCachingEnabled

        public bool IsCachingEnabled
        {
            get => (bool)GetValue(IsCachingEnabledProperty);
            set => SetValue(IsCachingEnabledProperty, value);
        }

        public static readonly DependencyProperty IsCachingEnabledProperty =
            DependencyProperty.Register("IsCachingEnabled", typeof(bool), typeof(AnimatedImage2), new PropertyMetadata(true, OnPropertyChanged));

        #endregion

        #region FrameSize

        public Size FrameSize
        {
            get => (Size)GetValue(FrameSizeProperty);
            set => SetValue(FrameSizeProperty, value);
        }

        public static readonly DependencyProperty FrameSizeProperty =
            DependencyProperty.Register("FrameSize", typeof(Size), typeof(AnimatedImage2), new PropertyMetadata(new Size(256, 256), OnPropertyChanged));

        #endregion

        #region DecodeFrameType

        public DecodePixelType DecodeFrameType
        {
            get { return (DecodePixelType)GetValue(DecodeFrameTypeProperty); }
            set { SetValue(DecodeFrameTypeProperty, value); }
        }

        public static readonly DependencyProperty DecodeFrameTypeProperty =
            DependencyProperty.Register("DecodeFrameType", typeof(DecodePixelType), typeof(AnimatedImage2), new PropertyMetadata(DecodePixelType.Physical, OnPropertyChanged));

        #endregion

        private AnimatedImagePresentation GetPresentation()
        {
            if (Source != null)
            {
                var width = (int)FrameSize.Width;
                var height = (int)FrameSize.Height;

                if (DecodeFrameType == DecodePixelType.Logical)
                {
                    width = (int)(width * _rasterizationScale);
                    height = (int)(height * _rasterizationScale);
                }

                return new AnimatedImagePresentation(Source, width, height, true, LoopCount, AutoPlay, IsCachingEnabled);
            }

            return null;
        }

        private static void OnPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            ((AnimatedImage2)d).Prepare();
        }

        private void Prepare()
        {
            if (_templateApplied && _loaded > 0)
            {
                var presentation = GetPresentation();
                if (presentation != _presenter?.Presentation)
                {
                    if (_presenter != null)
                    {
                        _presenter.Unload(Canvas);
                        _presenter = null;
                    }

                    _delayedPlay = _state == PlayingState.Playing;
                    _state = PlayingState.None;

                    if (presentation != null)
                    {
                        _presenter = AnimatedImageLoader.Current.GetOrCreate(presentation);
                        _presenter.Load(Canvas);

                        if (_delayedPlay)
                        {
                            Play();
                        }
                    }
                }
            }
        }

        private void Unload()
        {
            if (_loaded <= 0)
            {
                _presenter?.Unload(Canvas);
                _presenter = null;
            }
        }

        private ImageBrush Canvas;

        protected override void OnApplyTemplate()
        {
            //Logger.Debug();
            Canvas = GetTemplateChild(nameof(Canvas)) as ImageBrush;
            _templateApplied = true;
            _rasterizationScale = XamlRoot.RasterizationScale;

            Prepare();
            base.OnApplyTemplate();
        }

        private void OnRasterizationScaleChanged(XamlRoot sender, XamlRootChangedEventArgs args)
        {
            if (_rasterizationScale != sender.RasterizationScale && DecodeFrameType == DecodePixelType.Logical)
            {
                _rasterizationScale = sender.RasterizationScale;
                Prepare();
            }
        }

        #region Legacy

        public bool IsLoopingEnabled => LoopCount == 0;

        bool IPlayerView.Play()
        {
            Play();
            return true;
        }

        void IPlayerView.Unload()
        {
            Pause();
        }

        #endregion
    }

    public class AnimatedImagePresenter
    {
        private bool _ticking;
        private bool _rendering;
        private bool _disposing;
        private bool _disposed;

        private int _loaded;

        private int _playing;
        private bool _idle = true;

        private bool _activated;

        private int _loopCount;

        private LoopThread _timer;

        private readonly AnimatedImagePresentation _presentation;
        private readonly AnimatedImageLoader _loader;
        private readonly DispatcherQueue _dispatcherQueue;

        private readonly List<ImageBrush> _images = new();

        private AnimatedImageTask _task;
        private readonly object _lock = new();

        public AnimatedImagePresenter(AnimatedImagePresentation configuration)
        {
            _presentation = configuration;
            _loader = AnimatedImageLoader.Current;

            _dispatcherQueue = DispatcherQueue.GetForCurrentThread();
        }

        public AnimatedImagePresentation Presentation => _presentation;

        public void Load(ImageBrush canvas)
        {
            _images.Add(canvas);
            Task.Run(PrepareImpl);
        }

        public void Unload(ImageBrush canvas)
        {
            _images.Remove(canvas);
            Task.Run(UnloadImpl);

            canvas.ImageSource = null;
        }

        private void PrepareImpl()
        {
            lock (_lock)
            {
                _loaded++;

                if (_loaded == 1)
                {
                    //Logger.Debug("Loaded, requesting");
                    _loader.Load(this, _presentation);
                }
            }
        }

        private void UnloadImpl()
        {
            //Logger.Debug();

            lock (_lock)
            {
                _loaded--;

                if (_loaded <= 0)
                {
                    if (_task != null)
                    {
                        if (_ticking)
                        {
                            Logger.Debug("Task exists, and timer is attached");
                            _disposing = true;
                            _ticking = false;
                        }
                        else
                        {
                            Logger.Debug("Task exists, and timer is not attached");
                            Dispose();
                        }
                    }
                }
            }
        }

        public void Play()
        {
            Task.Run(PlayImpl);
        }

        public void Pause()
        {
            Task.Run(PauseImpl);
        }

        private void PlayImpl()
        {
            lock (_lock)
            {
                _playing++;
                _idle = false;

                if (_task != null && _playing == 1 && !_ticking && _loopCount >= 0)
                {
                    _rendering = true;
                    _dispatcherQueue.TryEnqueue(RegisterRendering);

                    _ticking = _activated;
                    _timer.Tick += OnTick;
                }
            }
        }

        private void PauseImpl()
        {
            lock (_lock)
            {
                _playing--;
                _idle = false;

                if (_playing == 0)
                {
                    _ticking = false;
                }
            }
        }

        public void Ready(AnimatedImageTask task)
        {
            Task.Run(() => ReadyImpl(task));
        }

        private void ReadyImpl(AnimatedImageTask task)
        {
            //Logger.Debug();

            lock (_lock)
            {
                if (_loaded > 0)
                {
                    _timer = LoopThreadPool.Get(task.Interval);
                    _task = task;

                    _rendering = true;

                    _dispatcherQueue.TryEnqueue(CreateResources);
                    _createdResourcesLock.Wait();

                    _ticking = (_idle && _presentation.AutoPlay) || (_playing > 0 && (_activated || _presentation.LoopCount > 0));
                    _timer.Tick += OnTick;
                    _idle = false;
                }
                else if (_task != null)
                {
                    if (_ticking)
                    {
                        Logger.Debug("Task exists, and timer is attached");
                        _disposing = true;
                        _ticking = false;
                    }
                    else
                    {
                        Logger.Debug("Task exists, and timer is not attached");
                        Dispose();
                    }
                }
            }
        }

        #region Resources

        private PixelBuffer _foregroundPrev;
        private PixelBuffer _foregroundNext;
        private readonly ConcurrentQueue<PixelBuffer> _backgroundQueue = new();

        private readonly SemaphoreSlim _createdResourcesLock = new(0, 1);

        private void CreateResources()
        {
            var width = _task.PixelWidth;
            var height = _task.PixelHeight;

            _backgroundQueue.Enqueue(new PixelBuffer(new WriteableBitmap(width, height)));
            _backgroundQueue.Enqueue(new PixelBuffer(new WriteableBitmap(width, height)));

            _activated = Window.Current.CoreWindow.ActivationMode != CoreWindowActivationMode.Deactivated;

            _createdResourcesLock.Release();

            RegisterEvents();
            RegisterRendering();
        }

        private void RegisterEvents()
        {
            Window.Current.CoreWindow.Activated += OnActivated;
        }

        private void UnregisterEvents()
        {
            Window.Current.CoreWindow.Activated -= OnActivated;
        }

        private async void OnActivated(CoreWindow sender, WindowActivatedEventArgs args)
        {
            if (_disposed)
            {
                UnregisterEvents();
                return;
            }

            var activated = args.WindowActivationState != CoreWindowActivationState.Deactivated;
            var subscribe = await Task.Run(() => Activated(activated));

            if (subscribe)
            {
                RegisterRendering();
            }
        }

        public bool Activated(bool active)
        {
            lock (_lock)
            {
                if (_activated != active)
                {
                    _activated = active;

                    if (_playing > 0 && !active)
                    {
                        _ticking = false;
                    }
                    else if (_task != null && _playing > 0 && !_ticking && _loopCount >= 0 && active)
                    {
                        //_dispatcherQueue.TryEnqueue(RegisterRendering);

                        _rendering = true;
                        _ticking = true;
                        _timer.Tick += OnTick;

                        return true;
                    }
                }
            }

            return false;
        }

        private void RegisterRendering()
        {
            CompositionTarget.Rendering += OnRendering;
        }

        #endregion

        private void OnTick(object sender, EventArgs e)
        {
            //Logger.Debug();

            lock (_lock)
            {
                OnTick(sender as LoopThread);
            }
        }

        private void OnTick(LoopThread sender)
        {
            if (_loaded > 0 && !_disposing && !_disposed)
            {
                NextFrame();
            }

            if (!_ticking)
            {
                Logger.Debug("-=");

                _rendering = false;
                sender.Tick -= OnTick;

                if (_disposing)
                {
                    Dispose();
                }
            }
        }

        #region Next frame

        private void NextFrame()
        {
            if (TryDequeueOrExchange(out PixelBuffer frame))
            {
                if (NextFrame(frame))
                {
                    var dropped = Interlocked.Exchange(ref _foregroundNext, frame);
                    if (dropped != null)
                    {
                        _backgroundQueue.Enqueue(dropped);
                    }
                }
                else
                {
                    _backgroundQueue.Enqueue(frame);
                }
            }
        }

        private bool NextFrame(PixelBuffer frame)
        {
            var state = _task.NextFrame(frame);

            if (state == AnimatedImageTaskState.Stop)
            {
                _loopCount = -1;
                _ticking = false;
            }
            else if (state == AnimatedImageTaskState.Loop)
            {
                _loopCount++;

                if (_loopCount >= _presentation.LoopCount && _presentation.LoopCount > 0)
                {
                    _ticking = false;
                    _loopCount = 0;
                }
            }

            return state != AnimatedImageTaskState.Skip;
        }

        private bool TryDequeueOrExchange(out PixelBuffer frame)
        {
            if (_backgroundQueue.TryDequeue(out frame))
            {
                return true;
            }

            // TODO: this looks quite dangerous...
            frame = Interlocked.Exchange(ref _foregroundNext, null);
            return frame != null;
        }

        #endregion

        private void Dispose()
        {
            Logger.Debug();

            _dispatcherQueue.TryEnqueue(UnregisterEvents);

            _task.Dispose();
            _task = null;
            _timer = null;
            _disposing = false;
            _disposed = true;

            _foregroundPrev = null;
            _foregroundNext = null;
            _backgroundQueue.Clear();

            _loader.Remove(_presentation);

            Debug.Assert(_images.Count == 0);
        }

        protected TimeSpan _interval;
        protected TimeSpan _elapsed;

        private void OnRendering(object sender, object e)
        {
            //Logger.Debug();

            OnRendering(e);

            if (!_rendering)
            {
                Logger.Debug("-=");
                CompositionTarget.Rendering -= OnRendering;
            }
        }

        private void OnRendering(object e)
        {
            if (_interval.TotalMilliseconds > 17)
            {
                var args = e as RenderingEventArgs;
                var diff = args.RenderingTime - _elapsed;

                if (diff < _interval / 3 * 2 /*|| !_active*/)
                {
                    return;
                }

                _elapsed = args.RenderingTime;
            }

            DrawFrame();
        }

        private void DrawFrame()
        {
            var pixels = Interlocked.Exchange(ref _foregroundNext, null);
            if (pixels != null)
            {
                // We must check if the bitmap is still valid
                if (_foregroundPrev is PixelBuffer bitmap)
                {
                    _backgroundQueue.Enqueue(bitmap);
                }

                _foregroundPrev = pixels;
                pixels.Source.Invalidate();

                foreach (var image in _images)
                {
                    image.ImageSource = pixels.Source;
                }

                //DrawFrame(pixels.Visual);
            }
        }
    }

    public enum AnimatedImageTaskState
    {
        // All good
        None,

        // Buffer was not updated
        Skip,

        // Animation must stop right away
        Stop,

        // A cycle was completed
        Loop
    }

    public class LottieAnimatedImageTask : AnimatedImageTask
    {
        private readonly LottieAnimation _animation;

        public LottieAnimatedImageTask(LottieAnimation animation, AnimatedImagePresentation presentation)
            : base(presentation)
        {
            _animation = animation;

            PixelWidth = presentation.PixelWidth; //animation.PixelWidth;
            PixelHeight = presentation.PixelHeight; //animation.PixelHeight;

            var frameRate = Math.Clamp(animation.FrameRate, 30, presentation.LimitFps ? 30 : 60);
            var interval = TimeSpan.FromMilliseconds(Math.Floor(1000 / frameRate));

            Interval = interval;
        }

        private int _index;

        public override AnimatedImageTaskState NextFrame(PixelBuffer frame)
        {
            if (_animation.IsCaching)
            {
                return AnimatedImageTaskState.Skip;
            }

            var framesPerUpdate = _presentation.LimitFps ? _animation.FrameRate < 60 ? 1 : 2 : 1;

            _animation.RenderSync(frame, PixelWidth, PixelHeight, _index);
            _index = Math.Min(_animation.TotalFrame, _index + framesPerUpdate);

            if (_animation.TotalFrame == 1)
            {
                _index = 0;
                return AnimatedImageTaskState.Stop;
            }
            else if (_animation.TotalFrame == _index)
            {
                _index = 0;
                return AnimatedImageTaskState.Loop;
            }

            return AnimatedImageTaskState.None;
        }

        #region IDisposable

        private bool _disposed;

        public override void Dispose()
        {
            if (!_disposed)
            {
                _animation.Dispose();
                _disposed = true;
            }

            GC.SuppressFinalize(this);
        }

        #endregion
    }

    public class VideoAnimatedImageTask : AnimatedImageTask
    {
        private readonly CachedVideoAnimation _animation;

        public VideoAnimatedImageTask(CachedVideoAnimation animation, AnimatedImagePresentation presentation)
            : base(presentation)
        {
            _animation = animation;

            PixelWidth = presentation.PixelWidth; // animation.PixelWidth;
            PixelHeight = presentation.PixelHeight; // animation.PixelHeight;

            var frameRate = Math.Clamp(animation.FrameRate, 30, presentation.LimitFps ? 30 : 60);
            var interval = TimeSpan.FromMilliseconds(Math.Floor(1000 / frameRate));

            Interval = interval;
        }

        private int _index;

        public override AnimatedImageTaskState NextFrame(PixelBuffer frame)
        {
            if (_animation.IsCaching)
            {
                return AnimatedImageTaskState.Skip;
            }

            _animation.RenderSync(frame, PixelWidth, PixelHeight, out int seconds, out bool completed);
            _index++;

            if (_animation.TotalFrame == 1 || (completed && _index == 1))
            {
                _index = 0;
                return AnimatedImageTaskState.Stop;
            }
            else if (_animation.TotalFrame == _index || completed)
            {
                _index = 0;
                return AnimatedImageTaskState.Loop;
            }

            return AnimatedImageTaskState.None;
        }

        #region IDisposable

        private bool _disposed;

        public override void Dispose()
        {
            if (!_disposed)
            {
                _animation.Dispose();
                _disposed = true;
            }

            GC.SuppressFinalize(this);
        }

        #endregion
    }

    public class WebpAnimatedImageTask : AnimatedImageTask
    {
        private readonly IBuffer _animation;

        public WebpAnimatedImageTask(IBuffer animation, Size size, AnimatedImagePresentation presentation)
            : base(presentation)
        {
            _animation = animation;

            PixelWidth = (int)size.Width; // animation.PixelWidth;
            PixelHeight = (int)size.Height; // animation.PixelHeight;

            Interval = TimeSpan.FromMilliseconds(1000d / 30);
        }

        public override AnimatedImageTaskState NextFrame(PixelBuffer frame)
        {
            BufferSurface.Copy(_animation, frame);
            return AnimatedImageTaskState.Stop;
        }

        #region IDisposable

        private bool _disposed;

        public override void Dispose()
        {
            if (!_disposed)
            {
                //_animation.Dispose();
                _disposed = true;
            }

            GC.SuppressFinalize(this);
        }

        #endregion
    }

    public abstract class AnimatedImageTask : IDisposable
    {
        protected readonly AnimatedImagePresentation _presentation;

        protected AnimatedImageTask(AnimatedImagePresentation presentation)
        {
            _presentation = presentation;
        }

        public int PixelWidth { get; init; }
        public int PixelHeight { get; init; }

        public TimeSpan Interval { get; init; }

        public abstract AnimatedImageTaskState NextFrame(PixelBuffer frame);

        public abstract void Dispose();
    }

    public record AnimatedImagePresentation(AnimatedImageSource Source, int PixelWidth, int PixelHeight, bool LimitFps, int LoopCount, bool AutoPlay, bool IsCachingEnabled);

    public class AnimatedImageLoader
    {
        [ThreadStatic]
        private static AnimatedImageLoader _current;
        public static AnimatedImageLoader Current => _current ??= new();

        private readonly DispatcherQueue _dispatcherQueue;

        private AnimatedImageLoader()
        {
            _dispatcherQueue = DispatcherQueue.GetForCurrentThread();
        }

        private bool _workStarted;
        private Thread _workThread;

        private readonly WorkQueue _workQueue = new();
        private readonly object _workLock = new();

        //private static ConditionalWeakTable<AnimatedImage2, Delegate> _delegates = new();
        private readonly ConcurrentDictionary<Guid, WeakReference<AnimatedImagePresenter>> _delegates = new();
        private readonly Dictionary<AnimatedImagePresentation, AnimatedImagePresenter> _presenters = new();
        private readonly object _presentersLock = new();

        public AnimatedImagePresenter GetOrCreate(AnimatedImagePresentation configuration)
        {
            lock (_presentersLock)
            {
                if (_presenters.TryGetValue(configuration, out var presenter))
                {
                    return presenter;
                }

                presenter = new AnimatedImagePresenter(configuration);
                _presenters[configuration] = presenter;

                return presenter;
            }
        }

        public void Remove(AnimatedImagePresentation configuration)
        {
            lock (_presentersLock)
            {
                _presenters.Remove(configuration);
            }
        }

        public void Load(AnimatedImagePresenter sender, AnimatedImagePresentation configuration)
        {
            //Logger.Debug();

            var correlationId = Guid.NewGuid();

            _delegates[correlationId] = new WeakReference<AnimatedImagePresenter>(sender);
            _workQueue.Push(new WorkItem(correlationId, configuration));

            lock (_workLock)
            {
                if (_workStarted is false)
                {
                    if (_workThread?.IsAlive is false)
                    {
                        _workThread.Join();
                    }

                    _workStarted = true;
                    _workThread = new Thread(Work);
                    _workThread.Start();
                }
            }
        }

        private void Work()
        {
            while (_workStarted)
            {
                var work = _workQueue.WaitAndPop();
                if (work == null)
                {
                    _workStarted = false;
                    return;
                }

                if (work.Presentation.Source is LocalFileSource local)
                {
                    if (local.FilePath.EndsWith(".tgs"))
                    {
                        var animation = LottieAnimation.LoadFromFile(local.FilePath, new SizeInt32 { Width = work.Presentation.PixelWidth, Height = work.Presentation.PixelHeight }, true, null);
                        if (animation != null)
                        {
                            // TODO: check if animation is valid
                            // Width, height, frame rate...

                            if (TryGetDelegate(work.CorrelationId, out var target))
                            {
                                target.Ready(new LottieAnimatedImageTask(animation, work.Presentation));
                            }
                            else
                            {
                                animation.Dispose();
                            }
                        }
                    }
                    else if (local.FilePath.EndsWith(".webm"))
                    {
                        var animation = CachedVideoAnimation.LoadFromFile(local, work.Presentation.PixelWidth, work.Presentation.PixelHeight, work.Presentation.IsCachingEnabled);
                        if (animation != null)
                        {
                            // TODO: check if animation is valid
                            // Width, height, frame rate...

                            if (TryGetDelegate(work.CorrelationId, out var target))
                            {
                                target.Ready(new VideoAnimatedImageTask(animation, work.Presentation));
                            }
                            else
                            {
                                animation.Dispose();
                            }
                        }
                    }
                    else if (local.FilePath.EndsWith(".webp"))
                    {
                        var animation = PlaceholderImageHelper.Current.DrawWebP(local.FilePath, work.Presentation.PixelWidth, out Size size);
                        if (animation != null)
                        {
                            // TODO: check if animation is valid
                            // Width, height, frame rate...

                            if (TryGetDelegate(work.CorrelationId, out var target))
                            {
                                target.Ready(new WebpAnimatedImageTask(animation, size, work.Presentation));
                            }
                            else
                            {
                                //animation.Dispose();
                            }
                        }
                    }
                }
                else
                {
                    var animation = CachedVideoAnimation.LoadFromFile(work.Presentation.Source, work.Presentation.PixelWidth, work.Presentation.PixelHeight, work.Presentation.IsCachingEnabled);
                    if (animation != null)
                    {
                        // TODO: check if animation is valid
                        // Width, height, frame rate...

                        if (TryGetDelegate(work.CorrelationId, out var target))
                        {
                            target.Ready(new VideoAnimatedImageTask(animation, work.Presentation));
                        }
                        else
                        {
                            animation.Dispose();
                        }
                    }
                }
            }
        }

        private bool TryGetDelegate(Guid correlationId, out AnimatedImagePresenter target)
        {
            if (_delegates.TryRemove(correlationId, out var weak))
            {
                if (weak.TryGetTarget(out target))
                {
                    return true;
                }
            }

            target = null;
            return false;
        }

        record WorkItem(Guid CorrelationId, AnimatedImagePresentation Presentation);

        class WorkQueue
        {
            private readonly object _workAvailable = new();
            private readonly Queue<WorkItem> _work = new();

            public void Push(WorkItem item)
            {
                lock (_workAvailable)
                {
                    var was_empty = _work.Count == 0;

                    _work.Enqueue(item);

                    if (was_empty)
                    {
                        Monitor.Pulse(_workAvailable);
                    }
                }
            }

            public WorkItem WaitAndPop()
            {
                lock (_workAvailable)
                {
                    while (_work.Count == 0)
                    {
                        var timeout = Monitor.Wait(_workAvailable, 3000);
                        if (timeout is false)
                        {
                            return null;
                        }
                    }

                    return _work.Dequeue();
                }
            }
        }
    }
}
