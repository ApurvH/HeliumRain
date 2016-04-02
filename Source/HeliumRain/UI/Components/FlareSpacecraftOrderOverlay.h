#pragma once

#include "../../Flare.h"
#include "../../Data/FlareSpacecraftCatalog.h"
#include "../Components/FlareButton.h"
#include "../Components/FlareListItem.h"


class UFlareFactory;
class UFlareSimulatedSector;
class AFlareMenuManager;


DECLARE_DELEGATE_OneParam(FOrderDelegate, FFlareSpacecraftDescription*)


class SFlareSpacecraftOrderOverlay : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareSpacecraftOrderOverlay)
	{}

	SLATE_ARGUMENT(AFlareMenuManager*, MenuManager)

	SLATE_END_ARGS()


public:

	/*----------------------------------------------------
		Public methods
	----------------------------------------------------*/

	/** Create the widget */
	void Construct(const FArguments& InArgs);


	/*----------------------------------------------------
		Interaction
	----------------------------------------------------*/

	/** Show the overlay for a factory */
	void Open(UFlareFactory* Factory);
	
	/** Show the overlay for a sector */
	void Open(UFlareSimulatedSector* Sector, FOrderDelegate ConfirmationCallback);

	/** Close the overlay */
	void Close();


	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;


	/*----------------------------------------------------
		Content callbacks
	----------------------------------------------------*/
		
	/** Craft a line for the list */
	TSharedRef<ITableRow> OnGenerateSpacecraftLine(TSharedPtr<FInterfaceContainer> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** The selection was changed */
	void OnSpacecraftSelectionChanged(TSharedPtr<FInterfaceContainer> Item, ESelectInfo::Type SelectInfo);

	/** Can we confirm */
	EVisibility GetConfirmVisibility() const;


	/*----------------------------------------------------
		Action callbacks
	----------------------------------------------------*/

	/** Confirmed, start production */
	void OnConfirmed();

	/** Don't do anything and close */
	void OnClose();


protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	// State data
	UPROPERTY()
	TWeakObjectPtr<class AFlareMenuManager>                   MenuManager;
	FOrderDelegate                                            OnConfirmedCB;

	// Spacecraft building
	UFlareFactory*                                            TargetFactory;
	UFlareSimulatedSector*                                    TargetSector;
	TArray<TSharedPtr<FInterfaceContainer>>                   SpacecraftList;
	TSharedPtr<SListView<TSharedPtr<FInterfaceContainer>>>    SpacecraftSelector;

	// Slate data
	TSharedPtr<SFlareButton>                                  ConfirmButon;
	TSharedPtr<STextBlock>                                    ConfirmText;
	TSharedPtr<SFlareListItem>                                PreviousSelection;
	TSharedPtr<FInterfaceContainer>                           SelectedItem;

};
