/*----------------------------------------------------------------------------
 miniAudicle iOS
 iOS GUI to chuck audio programming environment
 
 Copyright (c) 2005-2012 Spencer Salazar.  All rights reserved.
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

#import "mADetailViewController.h"

#import "mAMasterViewController.h"
#import "mAChucKController.h"
#import "mATitleEditorController.h"
#import "mAVMMonitorController.h"
#import "mAConsoleMonitorController.h"
#import "miniAudicle.h"


@implementation mADetailItem

@synthesize title = _title;
@synthesize text = _text;
@synthesize docid = _docid;

+ (mADetailItem *)detailItemFromDictionary:(NSDictionary *)dictionary
{
    mADetailItem * detailItem = [mADetailItem new];
    
    detailItem.title = [dictionary objectForKey:@"title"];
    detailItem.text = [dictionary objectForKey:@"text"];
//    detailItem.docid = [[dictionary objectForKey:@"docid"] unsignedIntValue];
    detailItem.docid = [mAChucKController chuckController].ma->allocate_document_id();
    
    return detailItem;
}

- (NSDictionary *)dictionary
{
    NSMutableDictionary * dictionary = [NSMutableDictionary dictionary];
    
    [dictionary setObject:self.title forKey:@"title"];
    [dictionary setObject:self.text forKey:@"text"];
//    [dictionary setObject:[NSNumber numberWithUnsignedInt:self.docid] forKey:@"docid"];
    
    return [NSDictionary dictionaryWithDictionary:dictionary];
}

@end


@interface mADetailViewController ()

@property (strong, nonatomic) UIPopoverController *masterPopoverController;

@property (strong, nonatomic) UITextView * textView;
@property (strong, nonatomic) UIBarButtonItem * titleButton;
@property (strong, nonatomic) UIToolbar * toolbar;

@property (strong, nonatomic) UIPopoverController * popover;
@property (strong, nonatomic) mATitleEditorController * titleEditor;

@property (strong, nonatomic) UIPopoverController * vmMonitorPopover;
@property (strong, nonatomic) mAVMMonitorController * vmMonitor;

@property (strong, nonatomic) UIPopoverController * consoleMonitorPopover;
@property (strong, nonatomic) mAConsoleMonitorController * consoleMonitor;

- (void)configureView;

@end

@implementation mADetailViewController

@synthesize masterViewController = _masterViewController;

@synthesize detailItem = _detailItem;
@synthesize detailDescriptionLabel = _detailDescriptionLabel;
@synthesize masterPopoverController = _masterPopoverController;

@synthesize textView = _textView;
@synthesize titleButton = _titleButton, toolbar = _toolbar;

@synthesize popover = _popover, titleEditor = _titleEditor;
@synthesize vmMonitorPopover = _vmMonitorPopover, vmMonitor = _vmMonitor;
@synthesize consoleMonitorPopover = _consoleMonitorPopover, consoleMonitor = _consoleMonitor;

#pragma mark - Managing the detail item

- (void)setDetailItem:(id)newDetailItem
{
    if(_detailItem != newDetailItem)
    {
        if(_detailItem)
        {
            // save text
            _detailItem.text = self.textView.text;
        }
        
        _detailItem = newDetailItem;
        
        // Update the view.
        [self configureView];
    }

    if (self.masterPopoverController != nil) {
        [self.masterPopoverController dismissPopoverAnimated:YES];
    }        
}

- (void)configureView
{
    // Update the user interface for the detail item.

    if (self.detailItem)
    {
        self.titleButton.title = self.detailItem.title;
        self.textView.text = self.detailItem.text;
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    [self configureView];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        return (interfaceOrientation != UIInterfaceOrientationPortraitUpsideDown);
    } else {
        return YES;
    }
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = NSLocalizedString(@"Detail", @"Detail");
    }
    return self;
}
							
#pragma mark - Split view

- (void)splitViewController:(UISplitViewController *)splitController 
     willHideViewController:(UIViewController *)viewController 
          withBarButtonItem:(UIBarButtonItem *)barButtonItem 
       forPopoverController:(UIPopoverController *)popoverController
{
    barButtonItem.title = NSLocalizedString(@"Scripts", @"Scripts");
    
//    [self.titleButton setLeftBarButtonItem:barButtonItem animated:YES];
    NSMutableArray * items = [NSMutableArray arrayWithArray:self.toolbar.items];
    [items insertObject:barButtonItem atIndex:0];
    [self.toolbar setItems:items animated:YES];
    
    self.masterPopoverController = popoverController;
}

- (void)splitViewController:(UISplitViewController *)splitController 
     willShowViewController:(UIViewController *)viewController 
  invalidatingBarButtonItem:(UIBarButtonItem *)barButtonItem
{
    // Called when the view is shown again in the split view, invalidating the button and popover controller.
//    [self.titleButton setLeftBarButtonItem:nil animated:YES];    
    NSMutableArray * items = [NSMutableArray arrayWithArray:self.toolbar.items];
    [items removeObject:barButtonItem];
    [self.toolbar setItems:items animated:YES];

    
    self.masterPopoverController = nil;
}


#pragma mark miniAudicle / ChucK VM stuff

- (void)saveScript
{
    self.detailItem.text = self.textView.text;
}


- (IBAction)newScript:(id)sender
{
    [self.masterViewController newScript];
}


- (IBAction)addShred
{
    if(self.detailItem == nil) return;
    
    std::string code = [self.textView.text UTF8String];
    std::string name = [self.detailItem.title UTF8String];
    vector<string> args;
    t_CKUINT shred_id;
    std::string output;
    
    [mAChucKController chuckController].ma->run_code(code, name, args, 
                                                     self.detailItem.docid, 
                                                     shred_id, output);
}


- (IBAction)replaceShred
{
    if(self.detailItem == nil) return;
    
    std::string code = [self.textView.text UTF8String];
    std::string name = [self.detailItem.title UTF8String];
    vector<string> args;
    t_CKUINT shred_id;
    std::string output;
    
    [mAChucKController chuckController].ma->replace_code(code, name, args, 
                                                         self.detailItem.docid, 
                                                         shred_id, output);
}


- (IBAction)removeShred
{
    if(self.detailItem == nil) return;
    
    t_CKUINT shred_id;
    std::string output;

    [mAChucKController chuckController].ma->remove_code(self.detailItem.docid, 
                                                        shred_id, output);
}


- (IBAction)editTitle:(id)sender
{
    if(self.popover == nil)
    {
        self.popover = [[UIPopoverController alloc] initWithContentViewController:self.titleEditor];
    }
    
    self.titleEditor.editedTitle = self.detailItem.title;
    self.titleEditor.delegate = self;
    self.popover.delegate = self;
    
    [self.popover presentPopoverFromBarButtonItem:self.titleButton
                         permittedArrowDirections:UIPopoverArrowDirectionUp
                                         animated:YES];
}


- (void)titleEditorDidConfirm:(mATitleEditorController *)titleEditor
{
    [self.popover dismissPopoverAnimated:YES];
    
    self.detailItem.title = self.titleEditor.editedTitle;
    self.detailItem.text = self.textView.text;
    
    [self configureView];
    
    [self.masterViewController scriptDetailChanged];
}


- (void)titleEditorDidCancel:(mATitleEditorController *)titleEditor
{
    [self.popover dismissPopoverAnimated:YES];
}


- (IBAction)showVMMonitor:(id)sender
{
    if(self.vmMonitorPopover == nil)
    {
        self.vmMonitorPopover = [[UIPopoverController alloc] initWithContentViewController:self.vmMonitor];
    }
    
    if(self.vmMonitorPopover.isPopoverVisible)
    {
        [self.vmMonitorPopover dismissPopoverAnimated:YES];
    }
    else
    {
        self.vmMonitorPopover.delegate = self;
        
        [self.vmMonitorPopover presentPopoverFromBarButtonItem:sender
                                      permittedArrowDirections:UIPopoverArrowDirectionUp
                                                      animated:YES];
    }
}

- (IBAction)showConsoleMonitor:(id)sender
{
    if(self.consoleMonitorPopover == nil)
    {
        self.consoleMonitorPopover = [[UIPopoverController alloc] initWithContentViewController:self.consoleMonitor];
    }
    
    if(self.consoleMonitorPopover.isPopoverVisible)
    {
        [self.consoleMonitorPopover dismissPopoverAnimated:YES];
    }
    else
    {
        self.consoleMonitorPopover.delegate = self;
        
        [self.consoleMonitorPopover presentPopoverFromBarButtonItem:sender
                                           permittedArrowDirections:UIPopoverArrowDirectionUp
                                                           animated:YES];
    }
}


@end
