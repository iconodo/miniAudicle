/*----------------------------------------------------------------------------
 miniAudicle
 Cocoa GUI to chuck audio programming environment
 
 Copyright (c) 2005-2013 Spencer Salazar.  All rights reserved.
 http://chuck.cs.princeton.edu/
 http://soundlab.cs.princeton.edu/
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 U.S.A.
 -----------------------------------------------------------------------------*/

/* Based in part on: */
//
//  MultiDocWindowController.m
//  MultiDocTest
//
//  Created by Cartwright Samuel on 3/14/13.
//  Copyright (c) 2013 Samuel Cartwright. All rights reserved.
//

/* Also based in part on: */
//
//  WindowController.m
//  PSMTabBarControl
//
//  Created by John Pannell on 4/6/06.
//  Copyright 2006 Positive Spin Media. All rights reserved.
//

#import "mAMultiDocWindowController.h"
#import "miniAudicleDocument.h"
#import "miniAudicleController.h"
#import "mADocumentViewController.h"
#import "miniAudiclePreferencesController.h"
#import <PSMTabBarControl/PSMTabStyle.h>

@interface mAMultiDocWindowController ()

@property (nonatomic,retain,readonly) NSMutableSet* documents;
@property (nonatomic,retain,readonly) NSMutableSet* contentViewControllers;

@end

@implementation mAMultiDocWindowController

@synthesize documents = _documents;
@synthesize contentViewControllers = _contentViewControllers;

- (NSMutableSet *)documents {
    if (!_documents) {
        _documents = [[NSMutableSet alloc] initWithCapacity:3];
    }
    return _documents;
}

- (NSMutableSet *)contentViewControllers {
    if (!_contentViewControllers) {
        _contentViewControllers = [[NSMutableSet alloc] initWithCapacity:3];
    }
    return _contentViewControllers;
}

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
        _showsToolbar = YES;
        _showsTabBar = YES;
        _vm_on = NO;
    }
    
    return self;
}

- (void)dealloc
{
    for(miniAudicleDocument * doc in self.documents)
    {
        [doc removeWindowController:self];
        [doc setWindowController:nil];
    }
    
    [[NSUserDefaultsController sharedUserDefaultsController]
     removeObserver:self
     forKeyPath:[NSString stringWithFormat:@"values.%@", mAPreferencesShowTabBar]];
    [[NSUserDefaultsController sharedUserDefaultsController]
     removeObserver:self
     forKeyPath:[NSString stringWithFormat:@"values.%@", mAPreferencesShowToolbar]];

    [_documents release];
    _documents = nil;
    [_contentViewControllers release];
    _contentViewControllers = nil;
    
    [super dealloc];
}

- (PSMTabBarControl *)tabBar
{
    return tabBar;
}

- (unsigned int)numberOfTabs
{
    return [tabView numberOfTabViewItems];
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
//    [tabBar retain];
    
    [tabBar hideTabBar:!_showsTabBar animate:NO];
    [tabBar setHideForSingleTab:!_showsTabBar];
    [tabBar setCanCloseOnlyTab:YES];
    [tabBar setSizeCellsToFit:YES];
    [tabBar setAllowsResizing:YES];
    [tabBar setAlwaysShowActiveTab:NO];
//    [tabBar setHideForSingleTab:YES];
    [tabBar setShowAddTabButton:YES];
    [tabBar setStyleNamed:@"Metal"];
    [[tabBar addTabButton] setTarget:self];
    [[tabBar addTabButton] setAction:@selector(newDocument:)];
    
    [[NSUserDefaultsController sharedUserDefaultsController]
     addObserver:self
     forKeyPath:[NSString stringWithFormat:@"values.%@", mAPreferencesShowTabBar]
     options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial
     context:nil];
    [[NSUserDefaultsController sharedUserDefaultsController]
     addObserver:self
     forKeyPath:[NSString stringWithFormat:@"values.%@", mAPreferencesShowToolbar]
     options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial
     context:nil];
    
// add views for any documents that were added before the window was created
    for(NSDocument* document in self.documents)
        [self addViewWithDocument:document tabViewItem:nil];
    
    [[self window] setShowsToolbarButton:YES];

//    NSButton * toolbar_pill = [[self window] standardWindowButton:NSWindowToolbarButton];
//    [toolbar_pill setTarget:self];
//    [toolbar_pill setAction:@selector(toggleToolbar:)];
    
    if([[self window] isMainWindow])
    {
        [[self window] setBackgroundColor:[NSColor colorWithSRGBRed:175.0/255.0
                                                              green:175.0/255.0
                                                               blue:175.0/255.0
                                                              alpha:1.0]];
    }
    else
    {
        [[self window] setBackgroundColor:[NSColor colorWithSRGBRed:223.0/255.0
                                                              green:223.0/255.0
                                                               blue:223.0/255.0
                                                              alpha:1.0]];
    }
}

- (void)addViewWithDocument:(NSDocument*)document tabViewItem:(NSTabViewItem *)tabViewItem
{
    mADocumentViewController *ctrl;

    if(tabViewItem == nil)
    {
        ctrl = (mADocumentViewController *)[(miniAudicleDocument *)document newPrimaryViewController];
        
        tabViewItem = [[[NSTabViewItem alloc] initWithIdentifier:ctrl] autorelease];
        [tabViewItem setView:ctrl.view];
        [tabViewItem setLabel:[document displayName]];
        
        NSUInteger tabIndex = [tabView numberOfTabViewItems];
        [tabView insertTabViewItem:tabViewItem atIndex:tabIndex];
        [tabView selectTabViewItem:tabViewItem];
    }
    else
    {
        ctrl = (mADocumentViewController *)[tabViewItem identifier];
    }
    
    [self.contentViewControllers addObject:ctrl];
    
    [ctrl activate];
    
    [document setWindow:self.window];
    [(miniAudicleDocument *)document setWindowController:self];
}

- (void)addDocument:(NSDocument *)docToAdd
{
    [self addDocument:docToAdd tabViewItem:nil];
}

- (void)addDocument:(NSDocument *)docToAdd tabViewItem:(NSTabViewItem *)tabViewItem
{
    NSMutableSet* documents = self.documents;
    if ([documents containsObject:docToAdd])
        return;
    
    [documents addObject:docToAdd];
    
    // check if the window has been created. We can not insert new tab
    // items until the nib has been loaded. So if the window isnt created
    // yet, do nothing and instead add the view controls during the
    // windowDidLoad function
    
    if(self.isWindowLoaded)
        [self addViewWithDocument:docToAdd tabViewItem:tabViewItem];
}

- (void)removeDocument:(NSDocument *)docToRemove
{
    [self removeDocument:docToRemove attachedToViewController:[(miniAudicleDocument *)docToRemove viewController]];
}

- (void)removeDocument:(NSDocument *)docToRemove attachedToViewController:(NSViewController*)ctrl
{
    NSMutableSet* documents = self.documents;
    if (![documents containsObject:docToRemove])
        return;
    
    // remove the document's view controller and view
    [ctrl.view removeFromSuperview];
    if ([ctrl respondsToSelector:@selector(setDocument:)])
        [(id)ctrl setDocument: nil];
    [ctrl release];
    
    // remove the view from the tab item
    // dont remove the tab view item from the tab view, as this is handled by the framework (when
    // we click on the close button on the tab) - of course it wouldnt be if you closed the document
    // using the menu (TODO)
    NSTabViewItem* tabViewItem = [tabView tabViewItemAtIndex:[tabView indexOfTabViewItemWithIdentifier:ctrl]];
    //[tabViewItem setView: nil];
    [tabView removeTabViewItem:tabViewItem];
    
    // remove the control from the view controllers set
    [self.contentViewControllers removeObject:ctrl];
    
    // finally detach the document from the window controller
    [docToRemove removeWindowController:self];
    [(miniAudicleDocument *)docToRemove setWindowController:nil];
    [documents removeObject:docToRemove];
}

- (void)document:(NSDocument *)doc wasEdited:(BOOL)edited;
{
    mADocumentViewController *viewController = [(miniAudicleDocument *)doc viewController];
    if(viewController == (mADocumentViewController *)[[tabView selectedTabViewItem] identifier])
        [[self window] setDocumentEdited:edited];
}

- (void)updateTitles
{
    NSTabViewItem *tabViewItem = [tabView selectedTabViewItem];
    NSViewController* ctrl = (NSViewController*)[tabViewItem identifier];
    NSDocument* doc = [(id)ctrl document];
    [[self window] setTitle:[doc displayName]];
    [tabViewItem setLabel:[doc displayName]];
}

- (void)setDocument:(NSDocument *)document
{
    // NSLog(@"Will not set document to: %@",document);
}

- (NSDocument *)document
{
    NSTabViewItem *tabViewItem = [tabView selectedTabViewItem];
    NSViewController* ctrl = (NSViewController*)tabViewItem.identifier;
    
    return [(id) ctrl document];
}


- (IBAction)closeTab:(id)sender
{
    NSViewController* ctrl = (NSViewController*)[[tabView selectedTabViewItem] identifier];
    NSDocument* doc = [(id)ctrl document];
    
    [doc canCloseDocumentWithDelegate:self
                  shouldCloseSelector:@selector(document:shouldClose:contextInfo:)
                          contextInfo:nil];
}

- (void)document:(NSDocument *)document shouldClose:(BOOL)shouldClose contextInfo:(void *)contextInfo
{
    if(shouldClose)
        [document close];
}

- (void)newDocument:(id)sender
{
    [[self window] makeKeyAndOrderFront:sender];
    [[NSDocumentController sharedDocumentController] newDocument:sender];
}

#pragma mark NSTabView + PSMTabBarControl delegate methods

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
    NSViewController* ctrl = (NSViewController*)[tabViewItem identifier];
    NSDocument* doc = [(id)ctrl document];
    [[self window] setDocumentEdited:[doc isDocumentEdited]];
    [[self window] setTitle:[doc displayName]];
}

- (BOOL)tabView:(NSTabView *)aTabView shouldCloseTabViewItem:(NSTabViewItem *)tabViewItem
{
    NSViewController* ctrl = (NSViewController*)[tabViewItem identifier];
    NSDocument* doc = [(id)ctrl document];
    
    [doc canCloseDocumentWithDelegate:self
                  shouldCloseSelector:@selector(document:shouldClose:contextInfo:)
                          contextInfo:nil];
    return NO;
}

- (void)tabView:(NSTabView *)aTabView willCloseTabViewItem:(NSTabViewItem *)tabViewItem
{
    NSLog(@"tabView willCloseTabViewItem");
}

- (void)tabView:(NSTabView *)aTabView didCloseTabViewItem:(NSTabViewItem *)tabViewItem
{
    NSLog(@"tabView didCloseTabViewItem");
}

- (void)tabView:(NSTabView *)aTabView didDetachTabViewItem:(NSTabViewItem *)tabViewItem
{
    NSLog(@"Did Detach Tab View Item");    
}

- (void)tabView:(NSTabView *)aTabView acceptedDraggingInfo:(id <NSDraggingInfo>)draggingInfo onTabViewItem:(NSTabViewItem *)tabViewItem
{
	NSLog(@"acceptedDraggingInfo: %@ onTabViewItem: %@", [[draggingInfo draggingPasteboard] stringForType:[[[draggingInfo draggingPasteboard] types] objectAtIndex:0]], [tabViewItem label]);
}

- (BOOL)tabView:(NSTabView*)aTabView shouldDragTabViewItem:(NSTabViewItem *)tabViewItem fromTabBar:(PSMTabBarControl *)tabBarControl
{
	return YES;
}

- (BOOL)tabView:(NSTabView*)aTabView shouldDropTabViewItem:(NSTabViewItem *)tabViewItem inTabBar:(PSMTabBarControl *)tabBarControl
{
//    NSLog(@"shouldDropTabViewItem: %@ inTabBar: %@", [tabViewItem label], tabBarControl);
    
	return YES;
}

- (void)tabView:(NSTabView*)aTabView didDropTabViewItem:(NSTabViewItem *)tabViewItem inTabBar:(PSMTabBarControl *)tabBarControl
{
	NSLog(@"didDropTabViewItem: %@ inTabBar: %@", [tabViewItem label], tabBarControl);
    
    mADocumentViewController *ctrl = (mADocumentViewController *)[tabViewItem identifier];
    miniAudicleDocument *doc = [ctrl document];

    [self.contentViewControllers removeObject:ctrl];
    [self.documents removeObject:doc];
    [doc removeWindowController:self];
    
    mAMultiDocWindowController * newWindowController = [[tabBarControl window] windowController];
    [newWindowController addDocument:doc tabViewItem:tabViewItem];
    [[tabBarControl window] makeKeyAndOrderFront:self];
    [ctrl activate];
    [[tabBarControl window] setDocumentEdited:[doc isDocumentEdited]];
}

- (NSImage *)tabView:(NSTabView *)aTabView imageForTabViewItem:(NSTabViewItem *)tabViewItem offset:(NSSize *)offset styleMask:(unsigned int *)styleMask
{
	// grabs whole window image
	NSImage *viewImage = [[[NSImage alloc] init] autorelease];
	NSRect contentFrame = [[[self window] contentView] frame];
	[[[self window] contentView] lockFocus];
	NSBitmapImageRep *viewRep = [[[NSBitmapImageRep alloc] initWithFocusedViewRect:contentFrame] autorelease];
	[viewImage addRepresentation:viewRep];
	[[[self window] contentView] unlockFocus];
	
    // grabs snapshot of dragged tabViewItem's view (represents content being dragged)
	NSView *viewForImage = [tabViewItem view];
	NSRect viewRect = [viewForImage frame];
	NSImage *tabViewImage = [[[NSImage alloc] initWithSize:viewRect.size] autorelease];
	[tabViewImage lockFocus];
	[viewForImage drawRect:[viewForImage bounds]];
	[tabViewImage unlockFocus];
	
	[viewImage lockFocus];
	NSPoint tabOrigin = [tabView frame].origin;
	tabOrigin.x += 10;
	tabOrigin.y += 13;
	[tabViewImage compositeToPoint:tabOrigin operation:NSCompositeSourceOver];
	[viewImage unlockFocus];
	
	//draw over where the tab bar would usually be
	NSRect tabFrame = [tabBar frame];
	[viewImage lockFocus];
	[[NSColor windowBackgroundColor] set];
	NSRectFill(tabFrame);
	//draw the background flipped, which is actually the right way up
	NSAffineTransform *transform = [NSAffineTransform transform];
	[transform scaleXBy:1.0 yBy:-1.0];
	[transform concat];
	tabFrame.origin.y = -tabFrame.origin.y - tabFrame.size.height;
	[(id <PSMTabStyle>)[tabBar style] drawBackgroundInRect:tabFrame];
	[transform invert];
	[transform concat];
	
	[viewImage unlockFocus];
	
	if ([tabBar orientation] == PSMTabBarHorizontalOrientation) {
		offset->width = [(id <PSMTabStyle>)[tabBar style] leftMarginForTabBarControl];
		offset->height = 22;
	} else {
		offset->width = 0;
		offset->height = 22 + [(id <PSMTabStyle>)[tabBar style] leftMarginForTabBarControl];
	}
	
	if (styleMask) {
		*styleMask = NSTitledWindowMask | NSTexturedBackgroundWindowMask;
	}
	
	return viewImage;
}

- (PSMTabBarControl *)tabView:(NSTabView *)aTabView newTabBarForDraggedTabViewItem:(NSTabViewItem *)tabViewItem atPoint:(NSPoint)point
{
	NSLog(@"newTabBarForDraggedTabViewItem: %@ atPoint: %@", [tabViewItem label], NSStringFromPoint(point));
	
	//create a new window controller with no tab items
	mAMultiDocWindowController *newWindowController = [(miniAudicleController *)[NSDocumentController sharedDocumentController] newWindowController];
	id <PSMTabStyle> style = (id <PSMTabStyle>)[tabBar style];
	
	NSRect windowFrame = [[newWindowController window] frame];
	point.y += windowFrame.size.height - [[[newWindowController window] contentView] frame].size.height;
	point.x -= [style leftMarginForTabBarControl];
	
	[[newWindowController window] setFrameTopLeftPoint:point];
	[[newWindowController tabBar] setStyle:style];
	
	return [newWindowController tabBar];
}


#pragma mark NSWindowDelegate

// Each document needs to be detached from the window controller before the window closes.
// In addition, any references to those documents from any child view controllers will also
// need to be cleared in order to ensure a proper cleanup.
// The windowWillClose: method does just that. One caveat found during debugging was that the
// window controller’s self pointer may become invalidated at any time within the method as
// soon as nothing else refers to it (using ARC). Since we’re disconnecting references to
// documents, there have been cases where the window controller got deallocated mid-way of
// cleanup. To prevent that, I’ve added a strong pointer to self and use that pointer exclusively
// in the windowWillClose: method.
- (void)windowWillClose:(NSNotification *)notification
{
    NSWindow * window = self.window;
    if (notification.object != window) {
        return;
    }
    
    // let's keep a reference to ourself and not have us thrown away while we clear out references.
    mAMultiDocWindowController* me = self;

    // detach the view controllers from the document first
    for (NSViewController* ctrl in me.contentViewControllers) {
        [ctrl.view removeFromSuperview];
        if ([ctrl respondsToSelector:@selector(setDocument:)]) {
            [(id) ctrl setDocument:nil];
            [ctrl release];
        }
    }
    
    // then any content view
    [window setContentView:nil];
    [me.contentViewControllers removeAllObjects];
       
    // disassociate this window controller from the document
    for (NSDocument* doc in me.documents) {
        [doc removeWindowController:me];
    }
    [me.documents removeAllObjects];
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
    [[self window] setBackgroundColor:[NSColor colorWithSRGBRed:175.0/255.0
                                                          green:175.0/255.0
                                                           blue:175.0/255.0
                                                          alpha:1.0]];
    [tabBar setNeedsDisplay];
}

- (void)windowDidResignMain:(NSNotification *)notification
{
    [[self window] setBackgroundColor:[NSColor colorWithSRGBRed:223.0/255.0
                                                          green:223.0/255.0
                                                           blue:223.0/255.0
                                                          alpha:1.0]];
    [tabBar setNeedsDisplay];
}

- (BOOL)windowShouldClose:(id)sender
{
    int numUnsaved = 0;
    
    for(NSDocument * doc in self.documents)
    {
        if([doc isDocumentEdited])
            numUnsaved++;
    }
    
    if(numUnsaved > 0)
    {
        NSString *messageText = [NSString stringWithFormat:@"You have %i miniAudicle documents with unsaved changes. Do you want to review these changes before quitting?",
                                 numUnsaved];
        
        NSAlert * alert = [NSAlert alertWithMessageText:messageText
                                          defaultButton:@"Review Changes..."
                                        alternateButton:@"Cancel"
                                            otherButton:@"Discard Changes"
                              informativeTextWithFormat:@"If you don’t review your documents, all your changes will be lost."];
        
        [alert beginSheetModalForWindow:[self window]
                          modalDelegate:self
                         didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                            contextInfo:nil];
        
        return NO;
    }
    else
    {
        return YES;
    }
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
    [[alert window] orderOut:self];
    
    if(returnCode == NSAlertDefaultReturn)
    {
        NSSet *documentsCopy = [[self.documents copy] autorelease];
        for(NSDocument * doc in documentsCopy)
        {
            [doc canCloseDocumentWithDelegate:self
                          shouldCloseSelector:@selector(document:shouldClose:contextInfo:)
                                  contextInfo:nil];
        }
    }
    else if(returnCode == NSAlertAlternateReturn)
    {
    }
    else if(returnCode == NSAlertOtherReturn)
    {
        [[self window] close];
    }
}


#pragma mark NSKeyValueObserving

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    NSLog(@"observeValueForKeyPath %@", keyPath);
    if([keyPath isEqualToString:[NSString stringWithFormat:@"values.%@", mAPreferencesShowTabBar]])
    {
        BOOL show = [[NSUserDefaults standardUserDefaults] boolForKey:mAPreferencesShowTabBar];
        [self setShowsTabBar:show];
    }
    else if([keyPath isEqualToString:[NSString stringWithFormat:@"values.%@", mAPreferencesShowToolbar]])
    {
        BOOL show = [[NSUserDefaults standardUserDefaults] boolForKey:mAPreferencesShowToolbar];
        [self setShowsToolbar:show];
    }
}


- (void)vm_on
{
    [self setVMOn:YES];
}

- (void)vm_off
{
    [self setVMOn:NO];
}

- (void)setVMOn:(BOOL)t_vm_on
{
    _vm_on = t_vm_on;
    
    for(NSToolbarItem * item in [[self.window toolbar] items])
    {
        [item setEnabled:_vm_on];
    }
}


#pragma mark OTF toolbar methods

- (void)add:(id)sender
{
    NSTabViewItem *tabViewItem = [tabView selectedTabViewItem];
    mADocumentViewController *vc = (mADocumentViewController *) tabViewItem.identifier;
    [vc add:sender];
}

- (void)remove:(id)sender
{
    NSTabViewItem *tabViewItem = [tabView selectedTabViewItem];
    mADocumentViewController *vc = (mADocumentViewController *) tabViewItem.identifier;
    [vc remove:sender];
}

- (void)replace:(id)sender
{
    NSTabViewItem *tabViewItem = [tabView selectedTabViewItem];
    mADocumentViewController *vc = (mADocumentViewController *) tabViewItem.identifier;
    [vc replace:sender];
}

- (void)removeall:(id)sender
{
    NSTabViewItem *tabViewItem = [tabView selectedTabViewItem];
    mADocumentViewController *vc = (mADocumentViewController *) tabViewItem.identifier;
    [vc removeall:sender];
}

- (void)removelast:(id)sender
{
    NSTabViewItem *tabViewItem = [tabView selectedTabViewItem];
    mADocumentViewController *vc = (mADocumentViewController *) tabViewItem.identifier;
    [vc removelast:sender];
}

- (void)setShowsTabBar:(BOOL)_stb
{
    _showsTabBar = _stb;
    [tabBar setHideForSingleTab:!_showsTabBar];
    [tabBar hideTabBar:!_showsTabBar animate:YES];
}

- (BOOL)showsToolbar
{
    return _showsToolbar;
}

- (void)setShowsToolbar:(BOOL)_stb
{
    _showsToolbar = _stb;
    [_toolbar setVisible:_showsToolbar];
}

- (BOOL)showsTabBar
{
    return _showsTabBar;
}


#pragma mark NSMenuValidation

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if([[menuItem title] isEqualToString:@"Close Tab"])
        return [[self window] isKeyWindow];
    else
        return YES;
}


#pragma mark NSToolbarDelegate implementation

- (BOOL)validateToolbarItem:(NSToolbarItem *)toolbar_item
{
    if( [toolbar_item tag] == 1 )
        return _vm_on;
    else
        return YES;
}

- (void)toggleToolbar:(id)sender
{
//    miniAudicleController * mac = [NSDocumentController sharedDocumentController];
//    [mac hideToolbar:sender];
    
    _showsToolbar = !_showsToolbar;
    [_toolbar setVisible:_showsToolbar];
}



@end
